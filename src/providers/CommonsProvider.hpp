#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QString>

namespace efcrawler {

// The best fit for "no probe needed": one request returns MIME, byte size,
// direct file URL, and a licence short name.
class CommonsProvider final : public SearchProvider
{
    Q_OBJECT

public:
    enum class FileKind {
        Any,
        Audio,
        Video,
        Bitmap
    };

    explicit CommonsProvider(QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

    void setFileKind(FileKind kind);

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    void processReply(QNetworkReply* reply);
    void releaseActiveReply();

    [[nodiscard]] static QString kindToken(FileKind kind);

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};
    QSet<QString> seenUrls_;
    FileKind kind_{FileKind::Any};
};

} // namespace efcrawler
