#pragma once

#include "../model/SearchResult.hpp"

#include <QObject>
#include <QString>

namespace efcrawler {

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

public slots:
    virtual void search(const QString& query) = 0;
    virtual void cancel() = 0;

signals:
    void resultFound(const efcrawler::SearchResult& result);
    void searchFinished();
    void providerUnavailable(const QString& provider,
                             const QString& reason);
    void providerError(const QString& provider,
                       const QString& message);
};

} // namespace efcrawler
