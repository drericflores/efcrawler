#include "WikipediaProvider.hpp"

#include "../model/ResourceClassifier.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

namespace efcrawler {
namespace {

// Wikimedia accepts a project URL in the User-Agent. Preferring that over the
// e-mail address reduces mail harvesting; the address remains in the About box
// and the README where it belongs.
constexpr auto kUserAgent =
    "eFCrawler/" EFCRAWLER_VERSION
    " (https://github.com/drericflores/efcrawler)";

} // namespace

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
    return activeReply_ != nullptr || stage_ != RequestStage::Idle;
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

void WikipediaProvider::releaseActiveReply()
{
    if (!activeReply_) {
        return;
    }

    QNetworkReply* reply = activeReply_;
    activeReply_ = nullptr;

    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void WikipediaProvider::cancel()
{
    bumpGeneration();

    // Releases the reply immediately rather than waiting for the aborted
    // finished() to arrive, so a search() on the same turn is not refused.
    releaseActiveReply();

    stage_ = RequestStage::Idle;

    // No searchFinished() here: this provider was cancelled, so the manager is
    // no longer waiting on it. Emitting would be read as the next query done.
}

QNetworkRequest WikipediaProvider::makeRequest(const QUrl& url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json");
    return request;
}

void WikipediaProvider::startSearchRequest(const QString& query)
{
    QUrl url(QStringLiteral("https://en.wikipedia.org/w/api.php"));

    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    parameters.addQueryItem(QStringLiteral("list"), QStringLiteral("search"));
    parameters.addQueryItem(QStringLiteral("srsearch"), query);
    parameters.addQueryItem(QStringLiteral("srlimit"), QStringLiteral("10"));
    parameters.addQueryItem(QStringLiteral("utf8"), QStringLiteral("1"));
    parameters.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    parameters.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));

    url.setQuery(parameters);

    stage_ = RequestStage::Search;

    const quint64 gen = generation();
    activeReply_ = network_.get(makeRequest(url));

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        if (gen != generation()) {
            return;
        }

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

void WikipediaProvider::processSearchReply(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        finish();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(name(), reply->errorString());
        finish();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit providerError(name(),
                           QStringLiteral("Invalid JSON response from Wikipedia."));
        finish();
        return;
    }

    const QJsonArray results = document.object()
        .value(QStringLiteral("query")).toObject()
        .value(QStringLiteral("search")).toArray();

    for (const QJsonValue& value : results) {
        const QString title =
            value.toObject().value(QStringLiteral("title")).toString().trimmed();

        if (title.isEmpty()) {
            continue;
        }

        pageTitles_.append(title);

        QString pageName = title;
        pageName.replace(QLatin1Char(' '), QLatin1Char('_'));

        const QUrl pageUrl(QStringLiteral("https://en.wikipedia.org/wiki/") + pageName);
        const QString canonical = pageUrl.toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }
        seenUrls_.insert(canonical);

        SearchResult result;
        result.title = title;
        result.type = ResourceType::Web;
        result.mime = QStringLiteral("text/html");
        result.provider = name();
        result.source = QStringLiteral("en.wikipedia.org");
        result.size = ResourceClassifier::formatSize(-1);
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
    QUrl url(QStringLiteral("https://en.wikipedia.org/w/api.php"));

    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    parameters.addQueryItem(QStringLiteral("prop"), QStringLiteral("extlinks"));
    parameters.addQueryItem(QStringLiteral("titles"),
                            pageTitles_.join(QLatin1Char('|')));
    // Was "max", which for a link-heavy article returns an unbounded response.
    // 100 keeps the payload predictable; full `continue` pagination is a
    // follow-up.
    parameters.addQueryItem(QStringLiteral("ellimit"), QStringLiteral("100"));
    parameters.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    parameters.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));

    url.setQuery(parameters);

    stage_ = RequestStage::ExternalLinks;

    const quint64 gen = generation();
    activeReply_ = network_.get(makeRequest(url));

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        if (gen != generation()) {
            return;
        }

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

void WikipediaProvider::processExternalLinksReply(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        finish();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(name(), reply->errorString());
        finish();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit providerError(name(),
                           QStringLiteral("Invalid external-link response "
                                          "from Wikipedia."));
        finish();
        return;
    }

    const QJsonArray pages = document.object()
        .value(QStringLiteral("query")).toObject()
        .value(QStringLiteral("pages")).toArray();

    for (const QJsonValue& pageValue : pages) {
        const QJsonObject page = pageValue.toObject();
        const QString pageTitle =
            page.value(QStringLiteral("title")).toString();

        const QJsonArray links = page.value(QStringLiteral("extlinks")).toArray();

        for (const QJsonValue& linkValue : links) {
            const QString rawUrl =
                linkValue.toObject().value(QStringLiteral("url")).toString().trimmed();

            const QUrl url(rawUrl);

            if (!url.isValid()) {
                continue;
            }

            const QString scheme = url.scheme().toLower();
            if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
                continue;
            }

            const TypeInfo info = ResourceClassifier::classify(url);

            // External Web links are skipped on purpose: they are mostly
            // citations, and the article itself was already emitted above.
            // Only downloadable / media links are worth surfacing here.
            if (info.type == ResourceType::Web || info.type == ResourceType::Unknown) {
                continue;
            }

            const QString canonical =
                url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);

            if (seenUrls_.contains(canonical)) {
                continue;
            }
            seenUrls_.insert(canonical);

            SearchResult result;
            result.title = url.fileName();
            if (result.title.isEmpty()) {
                result.title = QStringLiteral("%1 resource").arg(pageTitle);
            }

            result.type = info.type;
            result.mime = info.mime;
            result.downloadable = info.downloadable;
            result.streamable = info.streamable;
            result.ambiguous = info.ambiguous;
            result.provider = name();
            result.source = url.host();
            result.size = ResourceClassifier::formatSize(-1);
            result.url = url;

            emit resultFound(result);
        }
    }

    finish();
}

void WikipediaProvider::finish()
{
    activeReply_ = nullptr;
    stage_ = RequestStage::Idle;

    emit searchFinished();
}

} // namespace efcrawler
