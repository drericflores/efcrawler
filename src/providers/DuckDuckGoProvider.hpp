#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>

namespace efcrawler {

class DuckDuckGoProvider final : public SearchProvider
{
    Q_OBJECT

public:
    explicit DuckDuckGoProvider(QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    void processReply(QNetworkReply* reply);
    static QString decodeHtml(QString text);
    static QString stripHtml(QString text);
    static QUrl extractTarget(const QUrl& url);
    static QString classifyResource(const QUrl& url);
    static bool looksLikeChallenge(const QString& html);

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};
    QSet<QString> seenUrls_;
};

} // namespace efcrawler
