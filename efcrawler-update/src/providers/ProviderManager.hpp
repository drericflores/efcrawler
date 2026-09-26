#pragma once

#include "SearchProvider.hpp"

#include <QList>
#include <QString>

namespace efcrawler {

// Providers are injected via addProvider() instead of being hardcoded, so the
// engine can build a second chain for media sources.
class ProviderManager final : public SearchProvider
{
    Q_OBJECT

public:
    explicit ProviderManager(QObject* parent = nullptr);

    // Takes ownership. Call before search(); order defines the fallback chain.
    void addProvider(SearchProvider* provider);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isBusy() const noexcept override;

public slots:
    void search(const QString& query) override;
    void cancel() override;

private:
    void startCurrentProvider();
    void connectProvider(SearchProvider* provider);

    QList<SearchProvider*> providers_;
    int currentProvider_{-1};
    QString currentQuery_;
    bool busy_{false};
    bool providerFailed_{false};
    QString lastFailure_;
};

} // namespace efcrawler
