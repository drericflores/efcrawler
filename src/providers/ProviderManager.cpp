#include "ProviderManager.hpp"

#include "DuckDuckGoProvider.hpp"
#include "WikipediaProvider.hpp"

namespace efcrawler {

ProviderManager::ProviderManager(QObject* parent)
    : SearchProvider(parent)
{
    auto* duckDuckGo =
        new DuckDuckGoProvider(this);

    providers_.append(duckDuckGo);

    auto* wikipedia =
        new WikipediaProvider(this);

    providers_.append(wikipedia);

    connectProvider(duckDuckGo);
    connectProvider(wikipedia);
}

QString ProviderManager::name() const
{
    if (currentProvider_ >= 0 &&
        currentProvider_ < providers_.size()) {
        return providers_.at(currentProvider_)->name();
    }

    return QStringLiteral("Research Providers");
}

bool ProviderManager::isBusy() const noexcept
{
    return busy_;
}

void ProviderManager::search(const QString& query)
{
    if (busy_) {
        return;
    }

    currentQuery_ = query;
    currentProvider_ = 0;
    providerFailed_ = false;
    lastFailure_.clear();
    busy_ = true;

    startCurrentProvider();
}

void ProviderManager::cancel()
{
    if (!busy_) {
        return;
    }

    if (currentProvider_ >= 0 &&
        currentProvider_ < providers_.size()) {
        providers_.at(currentProvider_)->cancel();
    }

    busy_ = false;
}

void ProviderManager::startCurrentProvider()
{
    if (currentProvider_ < 0 ||
        currentProvider_ >= providers_.size()) {

        busy_ = false;

        emit providerUnavailable(
            QStringLiteral("Research Providers"),
            lastFailure_.isEmpty()
                ? QStringLiteral(
                      "No research provider is available.")
                : lastFailure_);

        emit searchFinished();
        return;
    }

    providerFailed_ = false;

    providers_.at(currentProvider_)->search(
        currentQuery_);
}

void ProviderManager::connectProvider(
    SearchProvider* provider)
{
    connect(
        provider,
        &SearchProvider::resultFound,
        this,
        &SearchProvider::resultFound);

    connect(
        provider,
        &SearchProvider::providerUnavailable,
        this,
        [this](const QString& providerName,
               const QString& reason) {
            providerFailed_ = true;

            lastFailure_ =
                QStringLiteral("%1 unavailable — %2")
                    .arg(providerName, reason);
        });

    connect(
        provider,
        &SearchProvider::providerError,
        this,
        [this](const QString& providerName,
               const QString& message) {
            providerFailed_ = true;

            lastFailure_ =
                QStringLiteral("%1 error — %2")
                    .arg(providerName, message);
        });

    connect(
        provider,
        &SearchProvider::searchFinished,
        this,
        [this]() {
            if (!busy_) {
                return;
            }

            if (providerFailed_) {
                ++currentProvider_;

                if (currentProvider_ <
                    providers_.size()) {
                    startCurrentProvider();
                    return;
                }

                busy_ = false;

                emit providerUnavailable(
                    QStringLiteral(
                        "Research Providers"),
                    lastFailure_);

                emit searchFinished();
                return;
            }

            busy_ = false;
            emit searchFinished();
        });
}

} // namespace efcrawler
