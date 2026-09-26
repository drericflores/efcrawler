#include "CommonsProvider.hpp"

#include "../model/ResourceClassifier.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

namespace efcrawler {
namespace {

constexpr auto kUserAgent =
    "eFCrawler/" EFCRAWLER_VERSION
    " (https://github.com/drericflores/efcrawler)";

} // namespace

CommonsProvider::CommonsProvider(QObject* parent)
    : SearchProvider(parent)
{
    network_.setTransferTimeout(20000);
}

QString CommonsProvider::name() const
{
    return QStringLiteral("Wikimedia Commons");
}

bool CommonsProvider::isBusy() const noexcept
{
    return activeReply_ != nullptr;
}

void CommonsProvider::setFileKind(FileKind kind)
{
    kind_ = kind;
}

QString CommonsProvider::kindToken(FileKind kind)
{
    switch (kind) {
    case FileKind::Audio:  return QStringLiteral("audio");
    case FileKind::Video:  return QStringLiteral("video");
    case FileKind::Bitmap: return QStringLiteral("bitmap");
    case FileKind::Any:    break;
    }
    return {};
}

void CommonsProvider::search(const QString& query)
{
    if (activeReply_) {
        return;
    }

    seenUrls_.clear();

    QString searchTerm = query;

    const QString token = kindToken(kind_);
    if (!token.isEmpty()) {
        searchTerm += QStringLiteral(" filetype:") + token;
    }

    QUrl url(QStringLiteral("https://commons.wikimedia.org/w/api.php"));

    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    parameters.addQueryItem(QStringLiteral("generator"), QStringLiteral("search"));
    parameters.addQueryItem(QStringLiteral("gsrsearch"), searchTerm);
    parameters.addQueryItem(QStringLiteral("gsrnamespace"), QStringLiteral("6"));
    parameters.addQueryItem(QStringLiteral("gsrlimit"), QStringLiteral("25"));
    parameters.addQueryItem(QStringLiteral("prop"), QStringLiteral("imageinfo"));
    parameters.addQueryItem(QStringLiteral("iiprop"),
                            QStringLiteral("url|mime|size|extmetadata"));
    parameters.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    parameters.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));

    url.setQuery(parameters);

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

void CommonsProvider::releaseActiveReply()
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

void CommonsProvider::cancel()
{
    bumpGeneration();
    releaseActiveReply();
}

void CommonsProvider::processReply(QNetworkReply* reply)
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
                           QStringLiteral("Invalid JSON response from Wikimedia Commons."));
        emit searchFinished();
        return;
    }

    const QJsonArray pages = document.object()
        .value(QStringLiteral("query")).toObject()
        .value(QStringLiteral("pages")).toArray();

    for (const QJsonValue& pageValue : pages) {
        const QJsonObject page = pageValue.toObject();

        const QJsonArray imageInfo = page.value(QStringLiteral("imageinfo")).toArray();
        if (imageInfo.isEmpty()) {
            continue;
        }

        const QJsonObject info = imageInfo.first().toObject();

        const QUrl url(info.value(QStringLiteral("url")).toString());
        if (!url.isValid() || url.isEmpty()) {
            continue;
        }

        const QString canonical =
            url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);

        if (seenUrls_.contains(canonical)) {
            continue;
        }
        seenUrls_.insert(canonical);

        const QString mime = info.value(QStringLiteral("mime")).toString();

        SearchResult result;
        result.title = page.value(QStringLiteral("title")).toString();
        result.title.remove(QStringLiteral("File:"));

        result.provider = name();
        result.source = QStringLiteral("commons.wikimedia.org");
        result.url = url;
        result.mime = mime;

        // Authoritative MIME and size -- no probe needed for these rows.
        const TypeInfo classified = ResourceClassifier::refine(
            ResourceClassifier::classify(url), mime);

        result.type = classified.type;
        result.downloadable = classified.downloadable;
        result.streamable = classified.streamable;

        // `size` arrives as a JSON number and can exceed 2 GB for a large
        // TIFF or video, so it must not go through toInt().
        const qint64 bytes =
            info.value(QStringLiteral("size")).toVariant().toLongLong();

        result.size = ResourceClassifier::formatSize(bytes);

        const QJsonObject extmetadata =
            info.value(QStringLiteral("extmetadata")).toObject();

        result.license = extmetadata.value(QStringLiteral("LicenseShortName"))
                             .toObject()
                             .value(QStringLiteral("value"))
                             .toString();

        result.licenseUrl = extmetadata.value(QStringLiteral("LicenseUrl"))
                                .toObject()
                                .value(QStringLiteral("value"))
                                .toString();

        emit resultFound(result);
    }

    emit searchFinished();
}

} // namespace efcrawler
