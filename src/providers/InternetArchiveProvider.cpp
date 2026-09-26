#include "InternetArchiveProvider.hpp"

#include "../model/ResourceClassifier.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>

namespace efcrawler {
namespace {

constexpr auto kUserAgent =
    "eFCrawler/" EFCRAWLER_VERSION
    " (https://github.com/drericflores/efcrawler)";

} // namespace

InternetArchiveProvider::InternetArchiveProvider(QObject* parent)
    : SearchProvider(parent)
{
    network_.setTransferTimeout(20000);
}

QString InternetArchiveProvider::name() const
{
    return QStringLiteral("Internet Archive");
}

bool InternetArchiveProvider::isBusy() const noexcept
{
    return activeReply_ != nullptr;
}

void InternetArchiveProvider::setMediatypeFilter(const QString& filter)
{
    mediatypeFilter_ = filter.trimmed();
}

void InternetArchiveProvider::search(const QString& query)
{
    if (activeReply_) {
        return;
    }

    seenUrls_.clear();

    QString qualified = QStringLiteral("(%1)").arg(query);

    if (!mediatypeFilter_.isEmpty()) {
        qualified += QStringLiteral(" AND mediatype:(%1)").arg(mediatypeFilter_);
    }

    // Built as a string rather than with QUrlQuery because the API's `fl[]`
    // parameter names contain brackets, which QUrlQuery percent-encodes.
    // VERIFY against the live endpoint on first run: if the response comes
    // back without `docs`, this encoding is the thing to look at.
    const QString full =
        QStringLiteral("https://archive.org/advancedsearch.php?q=")
        + QString::fromUtf8(QUrl::toPercentEncoding(qualified))
        + QStringLiteral("&fl[]=identifier&fl[]=title&fl[]=mediatype"
                         "&fl[]=year&fl[]=licenseurl"
                         "&rows=25&page=1&output=json");

    const QUrl url(full, QUrl::TolerantMode);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json");

    const quint64 gen = generation();
    activeReply_ = network_.get(request);

    connect(activeReply_, &QNetworkReply::finished, this, [this, gen]() {
        if (gen != generation()) {
            return;
        }

        QNetworkReply* reply = activeReply_;
        activeReply_ = nullptr;

        if (!reply) {
            emit searchFinished();
            return;
        }

        processReply(reply);
        reply->deleteLater();
    });
}

void InternetArchiveProvider::releaseActiveReply()
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

void InternetArchiveProvider::cancel()
{
    bumpGeneration();
    releaseActiveReply();
}

void InternetArchiveProvider::processReply(QNetworkReply* reply)
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

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit providerError(name(),
                           QStringLiteral("Invalid JSON response from Archive.org."));
        emit searchFinished();
        return;
    }

    const QJsonArray docs = document.object()
        .value(QStringLiteral("response")).toObject()
        .value(QStringLiteral("docs")).toArray();

    for (const QJsonValue& value : docs) {
        const QJsonObject doc = value.toObject();

        const QString identifier =
            doc.value(QStringLiteral("identifier")).toString().trimmed();

        if (identifier.isEmpty()) {
            continue;
        }

        // This is an item page, not a direct media file. Do not present the
        // page itself as an audio/video/PDF download.
        const QUrl url(QStringLiteral("https://archive.org/details/") + identifier);
        const QString canonical = url.toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }
        seenUrls_.insert(canonical);

        SearchResult result;
        result.title = doc.value(QStringLiteral("title")).toString().trimmed();
        if (result.title.isEmpty()) {
            result.title = identifier;
        }

        result.type = ResourceType::Media;
        result.provider = name();
        result.source = QStringLiteral("archive.org");
        result.size = ResourceClassifier::formatSize(-1);
        result.url = url;
        result.downloadable = false;

        // A reachable item page does not by itself grant reuse rights.
        result.licenseUrl = doc.value(QStringLiteral("licenseurl")).toString();
        if (!result.licenseUrl.isEmpty()) {
            result.license = result.licenseUrl;
        }

        const int year = doc.value(QStringLiteral("year")).toInt();
        if (year > 0) {
            result.title = QStringLiteral("%1 (%2)").arg(result.title).arg(year);
        }

        emit resultFound(result);
    }

    emit searchFinished();
}

} // namespace efcrawler
