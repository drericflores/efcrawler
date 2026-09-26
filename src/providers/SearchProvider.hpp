#pragma once

#include "../model/SearchResult.hpp"

#include <QObject>
#include <QString>

namespace efcrawler {

// Adds a generation counter. cancel() bumps it; reply handlers capture the
// value at request time and ignore any completion that arrives after it has
// moved on. That is what stops a cancelled request from being mistaken for
// the next one finishing.
class SearchProvider : public QObject
{
    Q_OBJECT

public:
    explicit SearchProvider(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~SearchProvider() override = default;

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual bool isBusy() const noexcept = 0;

    [[nodiscard]] quint64 generation() const noexcept { return generation_; }

public slots:
    virtual void search(const QString& query) = 0;
    virtual void cancel() = 0;

signals:
    void resultFound(const efcrawler::SearchResult& result);
    void searchFinished();
    void providerUnavailable(const QString& provider, const QString& reason);
    void providerError(const QString& provider, const QString& message);

protected:
    void bumpGeneration() { ++generation_; }

private:
    quint64 generation_{0};
};

} // namespace efcrawler
