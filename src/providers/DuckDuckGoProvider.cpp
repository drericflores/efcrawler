#include "DuckDuckGoProvider.hpp"

#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

namespace efcrawler {

DuckDuckGoProvider::DuckDuckGoProvider(QObject* parent)
    : SearchProvider(parent)
{
    network_.setTransferTimeout(20000);
}

QString DuckDuckGoProvider::name() const
{
    return QStringLiteral("DuckDuckGo");
}

bool DuckDuckGoProvider::isBusy() const noexcept
{
    return activeReply_ != nullptr;
}

void DuckDuckGoProvider::search(const QString& query)
{
    if (activeReply_) {
        return;
    }

    QUrl url(QStringLiteral("https://html.duckduckgo.com/html/"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("q"), query);

    QNetworkRequest request(url);

    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/x-www-form-urlencoded"));

    request.setRawHeader(
        "User-Agent",
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 "
        "(KHTML, like Gecko) "
        "Chrome/150.0 Safari/537.36");

    request.setRawHeader(
        "Accept",
        "text/html,application/xhtml+xml");

    request.setRawHeader(
        "Accept-Language",
        "en-US,en;q=0.9");

    const QByteArray body =
        form.query(QUrl::FullyEncoded).toUtf8();

    activeReply_ = network_.post(request, body);

    connect(
        activeReply_,
        &QNetworkReply::finished,
        this,
        [this]() {
            QNetworkReply* reply = activeReply_;
            activeReply_ = nullptr;

            if (!reply) {
                return;
            }

            processReply(reply);
            reply->deleteLater();
        });
}

void DuckDuckGoProvider::cancel()
{
    if (activeReply_) {
        activeReply_->abort();
    }
}

void DuckDuckGoProvider::processReply(QNetworkReply* reply)
{
    if (reply->error() ==
        QNetworkReply::OperationCanceledError) {
        emit searchFinished();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(
            name(),
            reply->errorString());

        emit searchFinished();
        return;
    }

    const QString html =
        QString::fromUtf8(reply->readAll());

    if (looksLikeChallenge(html)) {
        emit providerUnavailable(
            name(),
            QStringLiteral(
                "The provider returned an automated-search "
                "challenge instead of search results."));

        emit searchFinished();
        return;
    }

    static const QRegularExpression linkPattern(
        QStringLiteral(
            R"(<a[^>]*class=["'][^"']*result__a[^"']*["'][^>]*href=["']([^"']+)["'][^>]*>(.*?)</a>)"),
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatchIterator matches =
        linkPattern.globalMatch(html);

    while (matches.hasNext()) {
        const auto match = matches.next();

        QUrl url =
            extractTarget(
                QUrl::fromEncoded(
                    match.captured(1).toUtf8()));

        if (!url.isValid()) {
            continue;
        }

        const QString scheme =
            url.scheme().toLower();

        if (scheme != QStringLiteral("http") &&
            scheme != QStringLiteral("https")) {
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

        result.title =
            decodeHtml(
                stripHtml(match.captured(2)))
                .trimmed();

        if (result.title.isEmpty()) {
            result.title = url.fileName();
        }

        if (result.title.isEmpty()) {
            result.title = url.host();
        }

        result.type = classifyResource(url);
        result.source = url.host();
        result.size = QStringLiteral("—");
        result.access = QStringLiteral("Available");
        result.url = url;

        emit resultFound(result);
    }

    emit searchFinished();
}

bool DuckDuckGoProvider::looksLikeChallenge(
    const QString& html)
{
    const QString lower = html.toLower();

    const bool challenge =
        lower.contains(QStringLiteral("challenge")) &&
        (lower.contains(QStringLiteral("anomaly")) ||
         lower.contains(QStringLiteral("automated")));

    return challenge;
}

QString DuckDuckGoProvider::decodeHtml(QString text)
{
    text.replace(QStringLiteral("&amp;"),
                 QStringLiteral("&"));
    text.replace(QStringLiteral("&quot;"),
                 QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"),
                 QStringLiteral("'"));
    text.replace(QStringLiteral("&lt;"),
                 QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"),
                 QStringLiteral(">"));
    text.replace(QStringLiteral("&nbsp;"),
                 QStringLiteral(" "));

    return text;
}

QString DuckDuckGoProvider::stripHtml(QString text)
{
    static const QRegularExpression tags(
        QStringLiteral("<[^>]*>"));

    text.remove(tags);
    return text;
}

QUrl DuckDuckGoProvider::extractTarget(
    const QUrl& url)
{
    if (!url.isValid()) {
        return {};
    }

    if (url.host().contains(
            QStringLiteral("duckduckgo.com"),
            Qt::CaseInsensitive)) {

        QUrlQuery query(url);

        const QString target =
            query.queryItemValue(
                QStringLiteral("uddg"));

        if (!target.isEmpty()) {
            return QUrl::fromEncoded(
                target.toUtf8());
        }
    }

    return url;
}

QString DuckDuckGoProvider::classifyResource(
    const QUrl& url)
{
    const QString path =
        url.path().toLower();

    if (path.endsWith(QStringLiteral(".pdf"))) {
        return QStringLiteral("PDF");
    }

    if (path.endsWith(QStringLiteral(".docx")) ||
        path.endsWith(QStringLiteral(".doc"))) {
        return QStringLiteral("Document");
    }

    if (path.endsWith(QStringLiteral(".mp3"))) {
        return QStringLiteral("Audio");
    }

    if (path.endsWith(QStringLiteral(".mp4")) ||
        path.endsWith(QStringLiteral(".mpeg")) ||
        path.endsWith(QStringLiteral(".mpg"))) {
        return QStringLiteral("Video");
    }

    return QStringLiteral("Web");
}

} // namespace efcrawler
