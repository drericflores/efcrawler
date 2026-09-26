#include "DuckDuckGoProvider.hpp"

#include "../model/ResourceClassifier.hpp"

#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrlQuery>

namespace efcrawler {
namespace {

constexpr auto kBrowserUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/150.0 Safari/537.36";

} // namespace

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

    // Without this the set accumulated for the whole process lifetime, so every
    // URL surfaced by an earlier query -- or by an earlier research run in the
    // same session -- was silently dropped from later ones, and re-running the
    // same topic produced an empty table until restart. Cross-query dedup is
    // the engine's job (ResearchEngine::seenUrls_).
    seenUrls_.clear();

    QUrl url(QStringLiteral("https://html.duckduckgo.com/html/"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("q"), query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", kBrowserUserAgent);
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");

    const QByteArray body = form.query(QUrl::FullyEncoded).toUtf8();
    const quint64 gen = generation();

    activeReply_ = network_.post(request, body);

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        // Cancelled since the request went out: the manager is not waiting for
        // this one any more, so emitting searchFinished() here would be read as
        // the *next* query completing.
        if (gen != generation()) {
            return;
        }

        QNetworkReply* reply = activeReply_;
        activeReply_ = nullptr;

        if (!reply) {
            return;
        }

        processReply(reply);
        reply->deleteLater();
    });
}

void DuckDuckGoProvider::releaseActiveReply()
{
    if (!activeReply_) {
        return;
    }

    QNetworkReply* reply = activeReply_;

    // Released *before* abort(), so a search() arriving on the same event-loop
    // turn is not refused by the `if (activeReply_) return;` guard above.
    activeReply_ = nullptr;

    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void DuckDuckGoProvider::cancel()
{
    bumpGeneration();
    releaseActiveReply();
}

void DuckDuckGoProvider::processReply(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        emit searchFinished();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit providerError(name(), reply->errorString());
        emit searchFinished();
        return;
    }

    const QString html = QString::fromUtf8(reply->readAll());

    if (looksLikeChallenge(html)) {
        emit providerUnavailable(
            name(),
            QStringLiteral("The provider returned an automated-search "
                           "challenge instead of search results."));
        emit searchFinished();
        return;
    }

    static const QRegularExpression linkPattern(
        QStringLiteral(R"(<a[^>]*class=["'][^"']*result__a[^"']*["'][^>]*href=["']([^"']+)["'][^>]*>(.*?))"),
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatchIterator matches = linkPattern.globalMatch(html);

    while (matches.hasNext()) {
        const auto match = matches.next();

        const QUrl url =
            extractTarget(QUrl::fromEncoded(match.captured(1).toUtf8()));

        if (!url.isValid()) {
            continue;
        }

        const QString scheme = url.scheme().toLower();
        if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
            continue;
        }

        const QString canonical =
            url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }
        seenUrls_.insert(canonical);

        SearchResult result;
        result.title = plainTextFromHtml(match.captured(2));

        if (result.title.isEmpty()) {
            result.title = url.fileName();
        }
        if (result.title.isEmpty()) {
            result.title = url.host();
        }

        // One shared classifier instead of this provider's own copy.
        const TypeInfo info = ResourceClassifier::classify(url);

        result.type = info.type;
        result.mime = info.mime;
        result.downloadable = info.downloadable;
        result.streamable = info.streamable;
        result.ambiguous = info.ambiguous;
        result.provider = name();
        result.source = url.host();
        result.size = ResourceClassifier::formatSize(-1);
        result.url = url;

        // access is deliberately left Unknown -- LicenseGate decides in the
        // engine, so both providers agree on what "free" means.

        emit resultFound(result);
    }

    emit searchFinished();
}

bool DuckDuckGoProvider::looksLikeChallenge(const QString& html)
{
    const QString lower = html.toLower();

    return lower.contains(QStringLiteral("challenge")) &&
           (lower.contains(QStringLiteral("anomaly")) ||
            lower.contains(QStringLiteral("automated")));
}

QString DuckDuckGoProvider::plainTextFromHtml(QStringView html)
{
    // QTextDocumentFragment understands the full HTML entity set, including
    // the numeric forms the previous hand-rolled table could not cover, and
    // strips tags in the same pass. simplified() collapses the whitespace.
    return QTextDocumentFragment::fromHtml(html.toString())
        .toPlainText()
        .simplified();
}

QUrl DuckDuckGoProvider::extractTarget(const QUrl& url)
{
    if (!url.isValid()) {
        return {};
    }

    if (url.host().contains(QStringLiteral("duckduckgo.com"), Qt::CaseInsensitive)) {
        QUrlQuery query(url);

        const QString target = query.queryItemValue(QStringLiteral("uddg"));
        if (!target.isEmpty()) {
            return QUrl::fromEncoded(target.toUtf8());
        }
    }

    return url;
}

} // namespace efcrawler
