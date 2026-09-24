#pragma once

#include "SearchProvider.hpp"

#include <QList>

namespace efcrawler {

class ProviderManager final : public SearchProvider
{
    Q_OBJECT

public:
    explicit ProviderManager(QObject* parent = nullptr);

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
