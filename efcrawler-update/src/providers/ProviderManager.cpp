#include "ProviderManager.hpp"

namespace efcrawler {

ProviderManager::ProviderManager(QObject* parent)
    : SearchProvider(parent)
{
}

void ProviderManager::addProvider(SearchProvider* provider)
{
    if (!provider) {
        return;
    }

    provider->setParent(this);
    providers_.append(provider);
    connectProvider(provider);
}

QString ProviderManager::name() const
{
    if (currentProvider_ >= 0 && currentProvider_ < providers_.size()) {
        return providers_.at(currentProvider_)->name();
    }

    if (!providers_.isEmpty()) {
        return providers_.first()->name();
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
    if (currentProvider_ >= 0 && currentProvider_ < providers_.size()) {
        // Each child releases its own reply and bumps its own generation, so
        // no stale searchFinished() can arrive afterwards. Previously busy_
        // was dropped here while the child's activeReply_ stayed set, so the
        // next search() issued nothing and the aborted reply's completion was
        // counted as that new query finishing.
        providers_.at(currentProvider_)->cancel();
    }

    busy_ = false;
    currentProvider_ = -1;
}

void ProviderManager::startCurrentProvider()
{
    if (currentProvider_ < 0 || currentProvider_ >= providers_.size()) {
        busy_ = false;

        emit providerUnavailable(
            QStringLiteral("Research Providers"),
            lastFailure_.isEmpty()
                ? QStringLiteral("No research provider is available.")
                : lastFailure_);

        emit searchFinished();
        return;
    }

    providerFailed_ = false;

    providers_.at(currentProvider_)->search(currentQuery_);
}

void ProviderManager::connectProvider(SearchProvider* provider)
{
    connect(provider, &SearchProvider::resultFound,
            this, &SearchProvider::resultFound);

    connect(provider, &SearchProvider::providerUnavailable, this,
            [this](const QString& providerName, const QString& reason) {
                providerFailed_ = true;
                lastFailure_ = QStringLiteral("%1 unavailable — %2")
                                   .arg(providerName, reason);
            });

    connect(provider, &SearchProvider::providerError, this,
            [this](const QString& providerName, const QString& message) {
                providerFailed_ = true;
                lastFailure_ = QStringLiteral("%1 error — %2")
                                   .arg(providerName, message);
            });

    connect(provider, &SearchProvider::searchFinished, this, [this]() {
        if (!busy_) {
            return;
        }

        if (providerFailed_) {
            ++currentProvider_;

            if (currentProvider_ < providers_.size()) {
                startCurrentProvider();
                return;
            }

            busy_ = false;

            emit providerUnavailable(QStringLiteral("Research Providers"), lastFailure_);
            emit searchFinished();
            return;
        }

        busy_ = false;
        emit searchFinished();
    });
}

} // namespace efcrawler
