#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QString>
#include <QUrl>

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
    void releaseActiveReply();

    // Replaces a hand-rolled entity table plus a tag-stripping regex.
    [[nodiscard]] static QString plainTextFromHtml(QStringView html);
    [[nodiscard]] static QUrl extractTarget(const QUrl& url);
    [[nodiscard]] static bool looksLikeChallenge(const QString& html);

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};
    QSet<QString> seenUrls_;
};

} // namespace efcrawler
