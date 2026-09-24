#include "WikipediaProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace efcrawler {

WikipediaProvider::WikipediaProvider(QObject* parent)
    : SearchProvider(parent)
{
    network_.setTransferTimeout(20000);
}

QString WikipediaProvider::name() const
{
    return QStringLiteral("Wikipedia");
}

bool WikipediaProvider::isBusy() const noexcept
{
    return activeReply_ != nullptr ||
           stage_ != RequestStage::Idle;
}

void WikipediaProvider::search(const QString& query)
{
    if (isBusy()) {
        return;
    }

    pageTitles_.clear();
    seenUrls_.clear();

    startSearchRequest(query);
}

void WikipediaProvider::cancel()
{
    if (activeReply_) {
        activeReply_->abort();
        return;
    }

    finish();
}

void WikipediaProvider::startSearchRequest(
    const QString& query)
{
    QUrl url(QStringLiteral(
        "https://en.wikipedia.org/w/api.php"));

    QUrlQuery parameters;
    parameters.addQueryItem(
        QStringLiteral("action"),
        QStringLiteral("query"));
    parameters.addQueryItem(
        QStringLiteral("list"),
        QStringLiteral("search"));
    parameters.addQueryItem(
        QStringLiteral("srsearch"),
        query);
    parameters.addQueryItem(
        QStringLiteral("srlimit"),
        QStringLiteral("10"));
    parameters.addQueryItem(
        QStringLiteral("utf8"),
        QStringLiteral("1"));
    parameters.addQueryItem(
        QStringLiteral("format"),
        QStringLiteral("json"));
    parameters.addQueryItem(
        QStringLiteral("formatversion"),
        QStringLiteral("2"));

    url.setQuery(parameters);

    QNetworkRequest request(url);

    request.setRawHeader(
        "User-Agent",
        "eFCrawler/0.2.3 (eoftoro@gmail.com)");

    request.setRawHeader(
        "Accept",
        "application/json");

    stage_ = RequestStage::Search;
    activeReply_ = network_.get(request);

    connect(
        activeReply_,
        &QNetworkReply::finished,
        this,
        [this]() {
            QNetworkReply* reply = activeReply_;
            activeReply_ = nullptr;

            if (!reply) {
                finish();
                return;
            }

            processSearchReply(reply);
            reply->deleteLater();
        });
}

void WikipediaProvider::processSearchReply(
    QNetworkReply* reply)
{
    if (reply->error() ==
        QNetworkReply::OperationCanceledError) {
        finish();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(
            name(),
            reply->errorString());

        finish();
        return;
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            reply->readAll(),
            &parseError);

    if (parseError.error !=
            QJsonParseError::NoError ||
        !document.isObject()) {

        emit providerError(
            name(),
            QStringLiteral(
                "Invalid JSON response from Wikipedia."));

        finish();
        return;
    }

    const QJsonArray results =
        document.object()
            .value(QStringLiteral("query"))
            .toObject()
            .value(QStringLiteral("search"))
            .toArray();

    for (const QJsonValue& value : results) {
        const QJsonObject item = value.toObject();

        const QString title =
            item.value(QStringLiteral("title"))
                .toString()
                .trimmed();

        if (title.isEmpty()) {
            continue;
        }

        pageTitles_.append(title);

        QString pageName = title;
        pageName.replace(
            QLatin1Char(' '),
            QLatin1Char('_'));

        QUrl pageUrl(
            QStringLiteral(
                "https://en.wikipedia.org/wiki/") +
            pageName);

        const QString canonical =
            pageUrl.toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }

        seenUrls_.insert(canonical);

        SearchResult result;
        result.title = title;
        result.type = QStringLiteral("Web");
        result.source =
            QStringLiteral("en.wikipedia.org");
        result.size = QStringLiteral("—");
        result.access =
            QStringLiteral("Available");
        result.url = pageUrl;

        emit resultFound(result);
    }

    if (pageTitles_.isEmpty()) {
        finish();
        return;
    }

    startExternalLinksRequest();
}

void WikipediaProvider::startExternalLinksRequest()
{
    QUrl url(QStringLiteral(
        "https://en.wikipedia.org/w/api.php"));

    QUrlQuery parameters;
    parameters.addQueryItem(
        QStringLiteral("action"),
        QStringLiteral("query"));
    parameters.addQueryItem(
        QStringLiteral("prop"),
        QStringLiteral("extlinks"));
    parameters.addQueryItem(
        QStringLiteral("titles"),
        pageTitles_.join(QLatin1Char('|')));
    parameters.addQueryItem(
        QStringLiteral("ellimit"),
        QStringLiteral("max"));
    parameters.addQueryItem(
        QStringLiteral("format"),
        QStringLiteral("json"));
    parameters.addQueryItem(
        QStringLiteral("formatversion"),
        QStringLiteral("2"));

    url.setQuery(parameters);

    QNetworkRequest request(url);

    request.setRawHeader(
        "User-Agent",
        "eFCrawler/0.2.3 (eoftoro@gmail.com)");

    request.setRawHeader(
        "Accept",
        "application/json");

    stage_ = RequestStage::ExternalLinks;
    activeReply_ = network_.get(request);

    connect(
        activeReply_,
        &QNetworkReply::finished,
        this,
        [this]() {
            QNetworkReply* reply = activeReply_;
            activeReply_ = nullptr;

            if (!reply) {
                finish();
                return;
            }

            processExternalLinksReply(reply);
            reply->deleteLater();
        });
}

void WikipediaProvider::processExternalLinksReply(
    QNetworkReply* reply)
{
    if (reply->error() ==
        QNetworkReply::OperationCanceledError) {
        finish();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(
            name(),
            reply->errorString());

        finish();
        return;
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            reply->readAll(),
            &parseError);

    if (parseError.error !=
            QJsonParseError::NoError ||
        !document.isObject()) {

        emit providerError(
            name(),
            QStringLiteral(
                "Invalid external-link response "
                "from Wikipedia."));

        finish();
        return;
    }

    const QJsonArray pages =
        document.object()
            .value(QStringLiteral("query"))
            .toObject()
            .value(QStringLiteral("pages"))
            .toArray();

    for (const QJsonValue& pageValue : pages) {
        const QJsonObject page =
            pageValue.toObject();

        const QString pageTitle =
            page.value(QStringLiteral("title"))
                .toString();

        const QJsonArray links =
            page.value(QStringLiteral("extlinks"))
                .toArray();

        for (const QJsonValue& linkValue : links) {
            const QString rawUrl =
                linkValue.toObject()
                    .value(QStringLiteral("url"))
                    .toString()
                    .trimmed();

            QUrl url(rawUrl);

            if (!url.isValid()) {
                continue;
            }

            const QString scheme =
                url.scheme().toLower();

            if (scheme != QStringLiteral("http") &&
                scheme != QStringLiteral("https")) {
                continue;
            }

            const QString type =
                classifyResource(url);

            // The article itself already represents
            // ordinary Web content.  External discovery
            // concentrates on downloadable resources.
            if (type == QStringLiteral("Web")) {
                continue;
            }

            const QString canonical =
                url.adjusted(QUrl::RemoveFragment)
                    .toString(QUrl::FullyEncoded);

            if (seenUrls_.contains(canonical)) {
                continue;
            }

            seenUrls_.insert(canonical);

            SearchResult result;

            QString title = url.fileName();

            if (title.isEmpty()) {
                title =
                    QStringLiteral("%1 resource")
                        .arg(pageTitle);
            }

            result.title = title;
            result.type = type;
            result.source = url.host();
            result.size = QStringLiteral("—");
            result.access =
                QStringLiteral("Available");
            result.url = url;

            emit resultFound(result);
        }
    }

    finish();
}

QString WikipediaProvider::classifyResource(
    const QUrl& url)
{
    const QString path =
        url.path().toLower();

    if (path.endsWith(QStringLiteral(".pdf"))) {
        return QStringLiteral("PDF");
    }

    if (path.endsWith(QStringLiteral(".doc")) ||
        path.endsWith(QStringLiteral(".docx")) ||
        path.endsWith(QStringLiteral(".odt")) ||
        path.endsWith(QStringLiteral(".rtf"))) {
        return QStringLiteral("Document");
    }

    if (path.endsWith(QStringLiteral(".mp3")) ||
        path.endsWith(QStringLiteral(".wav")) ||
        path.endsWith(QStringLiteral(".ogg"))) {
        return QStringLiteral("Audio");
    }

    if (path.endsWith(QStringLiteral(".mp4")) ||
        path.endsWith(QStringLiteral(".mpeg")) ||
        path.endsWith(QStringLiteral(".mpg")) ||
        path.endsWith(QStringLiteral(".webm"))) {
        return QStringLiteral("Video");
    }

    return QStringLiteral("Web");
}

void WikipediaProvider::finish()
{
    activeReply_ = nullptr;
    stage_ = RequestStage::Idle;

    emit searchFinished();
}

} // namespace efcrawler
