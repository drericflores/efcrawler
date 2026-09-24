#pragma once

#include "../model/SearchResult.hpp"

#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

namespace efcrawler {

class SearchProvider;

class ResearchEngine final : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Searching,
        Paused
    };
    Q_ENUM(State)

    explicit ResearchEngine(QObject* parent = nullptr);

    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] QString topic() const;
    [[nodiscard]] int resultCount() const noexcept;
    [[nodiscard]] int queriesCompleted() const noexcept;
    [[nodiscard]] int totalQueries() const noexcept;
    [[nodiscard]] bool freeOnly() const noexcept;

    void setFreeOnly(bool enabled);

public slots:
    void startResearch(const QString& topic);
    void pauseResearch();
    void resumeResearch();
    void stopResearch();

signals:
    void researchStarted(const QString& topic);
    void researchPaused();
    void researchResumed();
    void researchStopped();

    void resultDiscovered(
        const efcrawler::SearchResult& result);

    void resultsCleared();

    void statusChanged(const QString& status);

    void progressChanged(
        int queriesCompleted,
        int totalQueries,
        int resultsFound);

    void providerStatusChanged(
        const QString& provider,
        const QString& status);

private:
    void buildResearchPlan();
    void dispatchNextQuery();
    void finishResearch();

    [[nodiscard]] bool looksCommercial(
        const SearchResult& result) const;

    [[nodiscard]] bool looksFree(
        const SearchResult& result) const;

    QString topic_;
    State state_{State::Idle};

    QQueue<QString> pendingQueries_;
    QSet<QString> seenUrls_;

    SearchProvider* provider_{nullptr};

    int queriesCompleted_{0};
    int totalQueries_{0};
    int resultCount_{0};

    bool providerAvailable_{true};
    bool stopRequested_{false};
    bool freeOnly_{true};
};

} // namespace efcrawler
