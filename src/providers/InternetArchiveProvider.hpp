#pragma once

#include "SearchProvider.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QString>

namespace efcrawler {

// API-based rather than scraped, and returns `licenseurl` -- which is what
// lets LicenseGate stop guessing at licences from substrings.
class InternetArchiveProvider final : public SearchProvider
{
    Q_OBJECT

public:
    explicit InternetArchiveProvider(QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

    // "audio", "movies", "image OR texts"; empty means no mediatype filter.
    void setMediatypeFilter(const QString& filter);

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    void processReply(QNetworkReply* reply);
    void releaseActiveReply();

    QNetworkAccessManager network_;
    QNetworkReply* activeReply_{nullptr};
    QSet<QString> seenUrls_;
    QString mediatypeFilter_;
};

} // namespace efcrawler
