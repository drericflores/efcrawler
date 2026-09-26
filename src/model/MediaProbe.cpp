#include "MediaProbe.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace efcrawler {
namespace {

constexpr int kTimeoutMs     = 8000;
constexpr int kMaxConcurrent = 3;

constexpr auto kUserAgent =
    "eFCrawler/" EFCRAWLER_VERSION
    " (https://github.com/drericflores/efcrawler)";

// Statuses that mean "no HEAD for you" rather than "no resource".
bool headIsRefused(int status)
{
    return status == 400 || status == 403 || status == 405 || status == 501;
}

} // namespace

MediaProbe::MediaProbe(QObject* parent)
    : QObject(parent)
{
    network_.setTransferTimeout(kTimeoutMs);
}

QString MediaProbe::canonicalKey(const QUrl& url)
{
    return url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
}

MediaInfo MediaProbe::cached(const QUrl& url) const
{
    return cache_.value(canonicalKey(url));
}

void MediaProbe::probe(const QUrl& url)
{
    if (!url.isValid() || url.isEmpty()) {
        return;
    }

    const QString key = canonicalKey(url);

    if (const auto it = cache_.constFind(key); it != cache_.constEnd()) {
        emit probed(url, it.value());   // replay for the newly selected row
        return;
    }

    if (queued_.contains(key) || active_.contains(key)) {
        return;
    }

    queue_.enqueue(url);
    queued_.insert(key);
    pump();
}

void MediaProbe::pump()
{
    while (active_.size() < kMaxConcurrent && !queue_.isEmpty()) {
        const QUrl url = queue_.dequeue();
        const QString key = canonicalKey(url);
        queued_.remove(key);

        auto state = QSharedPointer<State>::create();
        state->url = url;
        state->key = key;
        active_.insert(key, state);

        startHead(state);
    }
}

void MediaProbe::startHead(const QSharedPointer<State>& state)
{
    QNetworkRequest request(state->url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "*/*");

    state->reply = network_.head(request);

    connect(state->reply, &QNetworkReply::finished, this,
            [this, state]() {
                QNetworkReply* reply = state->reply;
                state->reply = nullptr;
                onFinished(reply, state);
                if (reply) {
                    reply->deleteLater();
                }
                pump();
            });
}

void MediaProbe::startRangeGet(const QSharedPointer<State>& state)
{
    // Some CDNs refuse HEAD. A one-byte ranged GET is the portable fallback;
    // we abort as soon as headers arrive, so no body is transferred even if
    // the server ignores the Range header.
    QNetworkRequest request(state->url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Range", "bytes=0-0");

    state->reply = network_.get(request);

    connect(state->reply, &QNetworkReply::metaDataChanged, this, [state]() {
        if (state->headerReady || !state->reply) {
            return;
        }
        state->headerReady = true;
        state->reply->abort();     // headers are all we wanted
    });

    connect(state->reply, &QNetworkReply::finished, this,
            [this, state]() {
                QNetworkReply* reply = state->reply;
                state->reply = nullptr;
                onFinished(reply, state);
                if (reply) {
                    reply->deleteLater();
                }
                pump();
            });
}

void MediaProbe::onFinished(QNetworkReply* reply, const QSharedPointer<State>& state)
{
    if (!reply) {
        return;
    }

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (headIsRefused(status) && !state->usedRange) {
        state->usedRange = true;
        startRangeGet(state);
        return;
    }

    // Our own abort after headers is a success, not a failure.
    const bool abortedAfterHeaders =
        state->headerReady &&
        reply->error() == QNetworkReply::OperationCanceledError;

    if (reply->error() != QNetworkReply::NoError && !abortedAfterHeaders && status == 0) {
        state->info.probed = true;
        state->info.ok = false;
        state->info.error = reply->errorString();
        finish(state);
        return;
    }

    state->info.probed = true;
    state->info.ok = true;
    state->info.mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    state->info.finalUrl = reply->url();
    state->info.suggestedFileName =
        fileNameFromDisposition(reply->rawHeader("Content-Disposition"));

    qint64 bytes = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();

    // A ranged reply carries the *total* size after the slash: "bytes 0-0/4096".
    if (state->usedRange) {
        const QString contentRange =
            QString::fromLatin1(reply->rawHeader("Content-Range"));

        const qsizetype slash = contentRange.indexOf(QLatin1Char('/'));
        if (slash >= 0) {
            const qint64 total = contentRange.mid(slash + 1).trimmed().toLongLong();
            if (total > 0) {
                bytes = total;
            }
        }
    }

    state->info.bytes = bytes > 0 ? bytes : -1;
    finish(state);
}

void MediaProbe::finish(const QSharedPointer<State>& state)
{
    active_.remove(state->key);
    cache_.insert(state->key, state->info);
    emit probed(state->url, state->info);
}

void MediaProbe::cancelAll()
{
    queue_.clear();
    queued_.clear();

    const auto states = active_.values();
    active_.clear();

    for (const auto& state : states) {
        if (state->reply) {
            QNetworkReply* reply = state->reply;
            state->reply = nullptr;
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            reply->deleteLater();
        }
    }
}

QString MediaProbe::fileNameFromDisposition(const QByteArray& header)
{
    if (header.isEmpty()) {
        return {};
    }

    const QString value = QString::fromUtf8(header);

    // RFC 5987 form first: filename*=UTF-8''My%20Song.mp3
    static const QRegularExpression extended(
        QStringLiteral(R"(filename\*\s*=\s*[^']*'[^']*'([^;]+))"),
        QRegularExpression::CaseInsensitiveOption);

    if (const auto match = extended.match(value); match.hasMatch()) {
        return QUrl::fromPercentEncoding(match.captured(1).trimmed().toUtf8());
    }

    // Plain form: filename="My Song.mp3"
    static const QRegularExpression plain(
        QStringLiteral(R"(filename\s*=\s*"?([^";]+)"?)"),
        QRegularExpression::CaseInsensitiveOption);

    if (const auto match = plain.match(value); match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    return {};
}

} // namespace efcrawler
