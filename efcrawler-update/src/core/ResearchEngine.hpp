#pragma once

#include "../model/SearchResult.hpp"

#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

namespace efcrawler {

class SearchProvider;

struct ResearchPlanOptions
{
    bool freeOnly{true};
    bool wantAudio{false};
    bool wantMovies{false};
    bool wantMedia{false};
};

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
    [[nodiscard]] ResearchPlanOptions options() const noexcept;

    void setFreeOnly(bool enabled);
    void setOptions(const ResearchPlanOptions& options);

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

    void resultDiscovered(const efcrawler::SearchResult& result);
    void resultsCleared();

    void statusChanged(const QString& status);

    void progressChanged(int queriesCompleted,
                         int totalQueries,
                         int resultsFound);

    void providerStatusChanged(const QString& provider,
                               const QString& status);

private:
    struct PlannedQuery {
        QString text;
        bool media{false};   // routed to the media chain, not the general one
    };

    void buildResearchPlan();
    void dispatchNextQuery();
    void finishResearch();

    void wireProvider(SearchProvider* provider, bool media);
    void handleResult(SearchResult result);

    QString topic_;
    State state_{State::Idle};

    QQueue<PlannedQuery> pendingQueries_;
    QSet<QString> seenUrls_;

    SearchProvider* general_{nullptr};   // DuckDuckGo -> Wikipedia
    SearchProvider* media_{nullptr};     // Internet Archive -> Wikimedia Commons
    SearchProvider* archive_{nullptr};   // kept so the mediatype filter can be set

    int queriesCompleted_{0};
    int totalQueries_{0};
    int resultCount_{0};

    bool generalAvailable_{true};
    bool mediaAvailable_{true};
    bool stopRequested_{false};
    bool pendingDispatch_{false};
    bool firstQuery_{true};

    // Jittered gap between consecutive queries against the scraped general
    // provider. Ten-plus back-to-back requests are what trip its challenge.
    int rateGapMs_{1400};

    ResearchPlanOptions options_;
};

} // namespace efcrawler
