#include "ResearchEngine.hpp"

#include "../providers/ProviderManager.hpp"
#include "../providers/SearchProvider.hpp"

#include <QStringList>

namespace efcrawler {

ResearchEngine::ResearchEngine(QObject* parent)
    : QObject(parent)
{
    provider_ = new ProviderManager(this);

    connect(
        provider_,
        &SearchProvider::resultFound,
        this,
        [this](SearchResult result) {
            const QString canonical =
                result.url
                    .adjusted(QUrl::RemoveFragment)
                    .toString(QUrl::FullyEncoded);

            if (seenUrls_.contains(canonical)) {
                return;
            }

            seenUrls_.insert(canonical);

            if (freeOnly_) {
                if (looksCommercial(result) &&
                    !looksFree(result)) {
                    return;
                }

                result.access =
                    looksFree(result)
                        ? QStringLiteral("Free candidate")
                        : QStringLiteral("Free/unknown");
            } else {
                result.access =
                    QStringLiteral("Web page");
            }

            ++resultCount_;

            emit resultDiscovered(result);

            emit progressChanged(
                queriesCompleted_,
                totalQueries_,
                resultCount_);
        });

    connect(
        provider_,
        &SearchProvider::providerUnavailable,
        this,
        [this](const QString& provider,
               const QString& reason) {
            providerAvailable_ = false;

            emit providerStatusChanged(
                provider,
                QStringLiteral("Unavailable"));

            emit statusChanged(
                QStringLiteral("%1 unavailable — %2")
                    .arg(provider, reason));
        });

    connect(
        provider_,
        &SearchProvider::providerError,
        this,
        [this](const QString& provider,
               const QString& message) {
            emit providerStatusChanged(
                provider,
                QStringLiteral("Error"));

            emit statusChanged(
                QStringLiteral("%1 error — %2")
                    .arg(provider, message));
        });

    connect(
        provider_,
        &SearchProvider::searchFinished,
        this,
        [this]() {
            if (stopRequested_) {
                return;
            }

            ++queriesCompleted_;

            emit progressChanged(
                queriesCompleted_,
                totalQueries_,
                resultCount_);

            if (!providerAvailable_) {
                pendingQueries_.clear();
                finishResearch();
                return;
            }

            if (state_ == State::Searching) {
                dispatchNextQuery();
            }
        });
}

ResearchEngine::State
ResearchEngine::state() const noexcept
{
    return state_;
}

QString ResearchEngine::topic() const
{
    return topic_;
}

int ResearchEngine::resultCount() const noexcept
{
    return resultCount_;
}

int ResearchEngine::queriesCompleted() const noexcept
{
    return queriesCompleted_;
}

int ResearchEngine::totalQueries() const noexcept
{
    return totalQueries_;
}

bool ResearchEngine::freeOnly() const noexcept
{
    return freeOnly_;
}

void ResearchEngine::setFreeOnly(bool enabled)
{
    freeOnly_ = enabled;
}

void ResearchEngine::startResearch(
    const QString& topic)
{
    const QString normalized = topic.trimmed();

    if (normalized.isEmpty()) {
        emit statusChanged(
            QStringLiteral("Enter a research topic."));
        return;
    }

    if (state_ != State::Idle) {
        stopResearch();
    }

    topic_ = normalized;

    pendingQueries_.clear();
    seenUrls_.clear();

    queriesCompleted_ = 0;
    resultCount_ = 0;

    providerAvailable_ = true;
    stopRequested_ = false;

    buildResearchPlan();

    state_ = State::Searching;

    emit resultsCleared();
    emit researchStarted(topic_);

    emit providerStatusChanged(
        provider_->name(),
        QStringLiteral("Ready"));

    emit progressChanged(
        0,
        totalQueries_,
        0);

    emit statusChanged(
        freeOnly_
            ? QStringLiteral(
                  "Free-resource research started: %1")
                  .arg(topic_)
            : QStringLiteral(
                  "Research started: %1")
                  .arg(topic_));

    dispatchNextQuery();
}

void ResearchEngine::pauseResearch()
{
    if (state_ != State::Searching) {
        return;
    }

    state_ = State::Paused;

    emit researchPaused();

    emit statusChanged(
        QStringLiteral(
            "Research paused. Current request may "
            "finish; no new request will start."));
}

void ResearchEngine::resumeResearch()
{
    if (state_ != State::Paused) {
        return;
    }

    state_ = State::Searching;

    emit researchResumed();

    emit statusChanged(
        QStringLiteral("Research resumed."));

    if (!provider_->isBusy()) {
        dispatchNextQuery();
    }
}

void ResearchEngine::stopResearch()
{
    if (state_ == State::Idle) {
        return;
    }

    stopRequested_ = true;
    state_ = State::Idle;

    pendingQueries_.clear();

    provider_->cancel();

    emit statusChanged(
        QStringLiteral(
            "Research stopped — %1 result(s) retained.")
            .arg(resultCount_));

    emit researchStopped();
}

void ResearchEngine::buildResearchPlan()
{
    if (freeOnly_) {
        pendingQueries_.enqueue(
            topic_ +
            QStringLiteral(" free download"));

        pendingQueries_.enqueue(
            topic_ +
            QStringLiteral(" free PDF"));

        pendingQueries_.enqueue(
            topic_ +
            QStringLiteral(" open access"));

        pendingQueries_.enqueue(
            topic_ +
            QStringLiteral(" public domain"));

        pendingQueries_.enqueue(
            QStringLiteral("\"%1\" free filetype:pdf")
                .arg(topic_));

        pendingQueries_.enqueue(
            topic_ +
            QStringLiteral(
                " free manual OR documentation"));
    } else {
        pendingQueries_.enqueue(topic_);

        pendingQueries_.enqueue(
            topic_ + QStringLiteral(" PDF"));

        pendingQueries_.enqueue(
            topic_ + QStringLiteral(" book"));

        pendingQueries_.enqueue(
            topic_ + QStringLiteral(" manual"));

        pendingQueries_.enqueue(
            QStringLiteral("\"%1\" filetype:pdf")
                .arg(topic_));

        pendingQueries_.enqueue(
            topic_ + QStringLiteral(" tutorial"));
    }

    totalQueries_ = pendingQueries_.size();
}

void ResearchEngine::dispatchNextQuery()
{
    if (state_ != State::Searching) {
        return;
    }

    if (!providerAvailable_) {
        finishResearch();
        return;
    }

    if (provider_->isBusy()) {
        return;
    }

    if (pendingQueries_.isEmpty()) {
        finishResearch();
        return;
    }

    const QString query =
        pendingQueries_.dequeue();

    emit providerStatusChanged(
        provider_->name(),
        QStringLiteral("Searching"));

    emit statusChanged(
        QStringLiteral("%1 — searching: %2")
            .arg(provider_->name(), query));

    provider_->search(query);
}

void ResearchEngine::finishResearch()
{
    if (state_ == State::Idle) {
        return;
    }

    state_ = State::Idle;

    if (!providerAvailable_) {
        emit statusChanged(
            QStringLiteral(
                "Research provider unavailable. "
                "%1 result(s) retained.")
                .arg(resultCount_));
    } else {
        emit providerStatusChanged(
            provider_->name(),
            QStringLiteral("Ready"));

        emit statusChanged(
            QStringLiteral(
                "Research complete — %1 result(s) found.")
                .arg(resultCount_));
    }

    emit researchStopped();
}

bool ResearchEngine::looksCommercial(
    const SearchResult& result) const
{
    const QString haystack =
        (result.title + QLatin1Char(' ') +
         result.source + QLatin1Char(' ') +
         result.url.toString())
            .toLower();

    static const QStringList indicators = {
        QStringLiteral("buy"),
        QStringLiteral("purchase"),
        QStringLiteral("pricing"),
        QStringLiteral("subscription"),
        QStringLiteral("subscribe"),
        QStringLiteral("checkout"),
        QStringLiteral("shopping"),
        QStringLiteral("cart"),
        QStringLiteral("store"),
        QStringLiteral("amazon."),
        QStringLiteral("ebay."),
        QStringLiteral("walmart."),
        QStringLiteral("barnesandnoble."),
        QStringLiteral("abebooks.")
    };

    for (const QString& indicator : indicators) {
        if (haystack.contains(indicator)) {
            return true;
        }
    }

    return false;
}

bool ResearchEngine::looksFree(
    const SearchResult& result) const
{
    const QString haystack =
        (result.title + QLatin1Char(' ') +
         result.source + QLatin1Char(' ') +
         result.url.toString())
            .toLower();

    static const QStringList indicators = {
        QStringLiteral("free"),
        QStringLiteral("open access"),
        QStringLiteral("public domain"),
        QStringLiteral("gutenberg"),
        QStringLiteral("archive.org"),
        QStringLiteral(".gov"),
        QStringLiteral(".edu"),
        QStringLiteral(".pdf")
    };

    for (const QString& indicator : indicators) {
        if (haystack.contains(indicator)) {
            return true;
        }
    }

    return false;
}

} // namespace efcrawler
