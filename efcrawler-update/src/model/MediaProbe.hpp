#pragma once

#include "ResourceType.hpp"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QSharedPointer>
#include <QUrl>

class QNetworkReply;

namespace efcrawler {

struct MediaInfo
{
    bool probed{false};
    bool ok{false};
    QString mime;
    qint64 bytes{-1};
    QString suggestedFileName;   // from Content-Disposition, if present
    QUrl finalUrl;
    QString error;
};

class MediaProbe final : public QObject
{
    Q_OBJECT

public:
    explicit MediaProbe(QObject* parent = nullptr);

    // Idempotent: a repeat call for the same URL replays the cached answer.
    void probe(const QUrl& url);

    [[nodiscard]] MediaInfo cached(const QUrl& url) const;

    void cancelAll();

signals:
    void probed(const QUrl& url, const efcrawler::MediaInfo& info);

private:
    struct State {
        QUrl url;
        QString key;
        QNetworkReply* reply{nullptr};
        bool usedRange{false};
        bool headerReady{false};
        MediaInfo info;
    };

    void pump();
    void startHead(const QSharedPointer<State>& state);
    void startRangeGet(const QSharedPointer<State>& state);
    void onFinished(QNetworkReply* reply, const QSharedPointer<State>& state);
    void finish(const QSharedPointer<State>& state);

    [[nodiscard]] static QString canonicalKey(const QUrl& url);
    [[nodiscard]] static QString fileNameFromDisposition(const QByteArray& header);

    QNetworkAccessManager network_;
    QQueue<QUrl> queue_;
    QSet<QString> queued_;
    QHash<QString, QSharedPointer<State>> active_;
    QHash<QString, MediaInfo> cache_;
};

} // namespace efcrawler

Q_DECLARE_METATYPE(efcrawler::MediaInfo)
