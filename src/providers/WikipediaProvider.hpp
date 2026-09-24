#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QStringList>

namespace efcrawler {

class WikipediaProvider final : public SearchProvider
{
    Q_OBJECT

public:
    explicit WikipediaProvider(QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    enum class RequestStage {
        Idle,
        Search,
        ExternalLinks
    };

    void startSearchRequest(const QString& query);
    void startExternalLinksRequest();

    void processSearchReply(QNetworkReply* reply);
    void processExternalLinksReply(QNetworkReply* reply);

    void finish();

    static QString classifyResource(const QUrl& url);

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};

    RequestStage stage_{RequestStage::Idle};

    QStringList pageTitles_;
    QSet<QString> seenUrls_;
};

} // namespace efcrawler
