#include "ResearchEngine.hpp"

#include "LicenseGate.hpp"
#include "../model/ResourceClassifier.hpp"
#include "../providers/CommonsProvider.hpp"
#include "../providers/DuckDuckGoProvider.hpp"
#include "../providers/InternetArchiveProvider.hpp"
#include "../providers/ProviderManager.hpp"
#include "../providers/SearchProvider.hpp"
#include "../providers/WikipediaProvider.hpp"

#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>

#include <utility>

namespace efcrawler {

ResearchEngine::ResearchEngine(QObject* parent)
    : QObject(parent)
{
    // --- general chain: scraped, so it gets the polite rate limit ---
    auto* general = new ProviderManager(this);
    general->addProvider(new DuckDuckGoProvider(general));
    general->addProvider(new WikipediaProvider(general));
    general_ = general;

    // --- media chain: documented JSON APIs, no scraping ---
    auto* media = new ProviderManager(this);

    archive_ = new InternetArchiveProvider(media);
    media->addProvider(archive_);
    media->addProvider(new CommonsProvider(media));
    media_ = media;

    wireProvider(general_, false);
    wireProvider(media_, true);
}

void ResearchEngine::wireProvider(SearchProvider* provider, bool media)
{
    connect(provider, &SearchProvider::resultFound,
            this, [this](SearchResult result) { handleResult(std::move(result)); });

    connect(provider, &SearchProvider::providerUnavailable, this,
            [this, media](const QString& name, const QString& reason) {
                if (media) {
                    mediaAvailable_ = false;
                } else {
                    generalAvailable_ = false;
                }

                emit providerStatusChanged(name, QStringLiteral("Unavailable"));
                emit statusChanged(QStringLiteral("%1 unavailable — %2")
                                       .arg(name, reason));
            });

    connect(provider, &SearchProvider::providerError, this,
            [this](const QString& name, const QString& message) {
                emit providerStatusChanged(name, QStringLiteral("Error"));
                emit statusChanged(QStringLiteral("%1 error — %2")
                                       .arg(name, message));
            });

    connect(provider, &SearchProvider::searchFinished, this, [this]() {
        if (stopRequested_) {
            return;
        }

        ++queriesCompleted_;

        emit progressChanged(queriesCompleted_, totalQueries_, resultCount_);

        if (state_ == State::Searching) {
            dispatchNextQuery();
        }
    });
}

void ResearchEngine::handleResult(SearchResult result)
{
    const QString canonical =
        result.url.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);

    if (seenUrls_.contains(canonical)) {
        return;
    }
    seenUrls_.insert(canonical);

    // Providers that carry authoritative metadata (Archive.org mediatype,
    // Commons MIME) set `type` themselves; everything else is classified here
    // so both providers agree on what a given URL is.
    if (result.type == ResourceType::Unknown) {
        const TypeInfo info = result.mime.isEmpty()
            ? ResourceClassifier::classify(result.url)
            : ResourceClassifier::refine(ResourceClassifier::classify(result.url),
                                         result.mime);

        result.type = info.type;
        result.downloadable = info.downloadable;
        result.streamable = info.streamable;
        result.ambiguous = info.ambiguous;

        if (result.mime.isEmpty()) {
            result.mime = info.mime;
        }
    }

    if (result.size.isEmpty()) {
        result.size = ResourceClassifier::formatSize(-1);
    }

    // Single decision point, replacing looksFree()/looksCommercial().
    result.access = LicenseGate::judge(result);

    if (!LicenseGate::shouldKeep(result, options_.freeOnly)) {
        return;
    }

    ++resultCount_;

    emit resultDiscovered(result);
    emit progressChanged(queriesCompleted_, totalQueries_, resultCount_);
}

ResearchEngine::State ResearchEngine::state() const noexcept
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
    return options_.freeOnly;
}

ResearchPlanOptions ResearchEngine::options() const noexcept
{
    return options_;
}

void ResearchEngine::setFreeOnly(bool enabled)
{
    options_.freeOnly = enabled;
}

void ResearchEngine::setOptions(const ResearchPlanOptions& options)
{
    options_ = options;
}

void ResearchEngine::startResearch(const QString& topic)
{
    const QString normalized = topic.trimmed();

    if (normalized.isEmpty()) {
        emit statusChanged(QStringLiteral("Enter a research topic."));
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

    generalAvailable_ = true;
    mediaAvailable_ = true;
    stopRequested_ = false;
    pendingDispatch_ = false;
    firstQuery_ = true;

    // Keep the media chain's mediatype filter in step with the checkboxes.
    if (auto* archive = dynamic_cast<InternetArchiveProvider*>(archive_)) {
        QStringList types;
        if (options_.wantAudio)  { types << QStringLiteral("audio"); }
        if (options_.wantMovies) { types << QStringLiteral("movies"); }
        if (options_.wantMedia)  { types << QStringLiteral("image") << QStringLiteral("texts"); }
        archive->setMediatypeFilter(types.join(QStringLiteral(" OR ")));
    }

    buildResearchPlan();

    state_ = State::Searching;

    emit resultsCleared();
    emit researchStarted(topic_);

    emit providerStatusChanged(general_->name(), QStringLiteral("Ready"));
    emit progressChanged(0, totalQueries_, 0);

    emit statusChanged(options_.freeOnly
        ? QStringLiteral("Free-resource research started: %1").arg(topic_)
        : QStringLiteral("Research started: %1").arg(topic_));

    dispatchNextQuery();
}

void ResearchEngine::pauseResearch()
{
    if (state_ != State::Searching) {
        return;
    }

    state_ = State::Paused;

    emit researchPaused();

    emit statusChanged(QStringLiteral(
        "Research paused. Current request may finish; no new request will start."));
}

void ResearchEngine::resumeResearch()
{
    if (state_ != State::Paused) {
        return;
    }

    state_ = State::Searching;

    emit researchResumed();
    emit statusChanged(QStringLiteral("Research resumed."));

    if (!general_->isBusy() && !media_->isBusy()) {
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

    general_->cancel();
    media_->cancel();

    emit statusChanged(QStringLiteral("Research stopped — %1 result(s) retained.")
                           .arg(resultCount_));

    emit researchStopped();
}

void ResearchEngine::buildResearchPlan()
{
    const QString t = topic_;
    const bool free = options_.freeOnly;

    const auto add = [this](const QString& text, bool media = false) {
        pendingQueries_.enqueue(PlannedQuery{ text, media });
    };

    // --- general ---
    if (free) {
        add(t + QStringLiteral(" free download"));
        add(t + QStringLiteral(" free PDF"));
        add(t + QStringLiteral(" open access"));
        add(t + QStringLiteral(" public domain"));
        add(QStringLiteral("\"%1\" free filetype:pdf").arg(t));
        add(t + QStringLiteral(" free manual OR documentation"));
    } else {
        add(t);
        add(t + QStringLiteral(" PDF"));
        add(t + QStringLiteral(" book"));
        add(t + QStringLiteral(" manual"));
        add(QStringLiteral("\"%1\" filetype:pdf").arg(t));
        add(t + QStringLiteral(" tutorial"));
    }

    // --- Audio ---
    if (options_.wantAudio) {
        if (free) {
            add(t + QStringLiteral(" filetype:mp3"));
            add(t + QStringLiteral(" public domain audio"));
            add(t + QStringLiteral(" podcast mp3 open license"));
            add(QStringLiteral("site:openverse.org \"%1\" audio").arg(t));
            add(QStringLiteral("\"%1\" creative commons music").arg(t), true);
            add(QStringLiteral("site:commons.wikimedia.org \"%1\" audio").arg(t), true);
            add(QStringLiteral("site:archive.org \"%1\" mediatype:audio").arg(t), true);
        } else {
            add(t + QStringLiteral(" mp3"));
            add(t + QStringLiteral(" audio download"));
            add(t + QStringLiteral(" song OR album OR soundtrack"));
            add(QStringLiteral("site:archive.org \"%1\" mediatype:audio").arg(t), true);
        }
    }

    // --- Movies ---
    if (options_.wantMovies) {
        if (free) {
            add(t + QStringLiteral(" filetype:mp4"));
            add(t + QStringLiteral(" public domain film"));
            add(t + QStringLiteral(" creative commons video"));
            add(t + QStringLiteral(" open movie download"));
            add(QStringLiteral("site:commons.wikimedia.org \"%1\" video").arg(t), true);
            add(QStringLiteral("site:archive.org \"%1\" mediatype:movies").arg(t), true);
        } else {
            add(t + QStringLiteral(" mp4"));
            add(t + QStringLiteral(" video download"));
            add(t + QStringLiteral(" trailer OR documentary OR film"));
            add(QStringLiteral("site:archive.org \"%1\" mediatype:movies").arg(t), true);
        }
    }

    // --- Media (images, galleries, item pages) ---
    if (options_.wantMedia) {
        if (free) {
            add(t + QStringLiteral(" public domain images"));
            add(t + QStringLiteral(" creative commons images"));
            add(QStringLiteral("site:openverse.org \"%1\"").arg(t), true);
            add(QStringLiteral("site:commons.wikimedia.org \"%1\" filetype:bitmap").arg(t), true);
        } else {
            add(t + QStringLiteral(" images"));
            add(t + QStringLiteral(" screenshots OR gallery"));
        }
    }

    totalQueries_ = pendingQueries_.size();
}

void ResearchEngine::dispatchNextQuery()
{
    if (state_ != State::Searching || pendingDispatch_) {
        return;
    }

    // Skip plan entries whose chain is unavailable, and wait if a chain is
    // still busy with the previous query.
    while (!pendingQueries_.isEmpty()) {
        const PlannedQuery& next = pendingQueries_.head();

        if (next.media && !mediaAvailable_) {
            pendingQueries_.dequeue();
            continue;
        }
        if (!next.media && !generalAvailable_) {
            pendingQueries_.dequeue();
            continue;
        }

        SearchProvider* provider = next.media ? media_ : general_;

        if (provider->isBusy()) {
            return;
        }
        break;
    }

    if (pendingQueries_.isEmpty()) {
        finishResearch();
        return;
    }

    const PlannedQuery next = pendingQueries_.dequeue();

    SearchProvider* provider = next.media ? media_ : general_;
    const QString query = next.text;

    emit providerStatusChanged(provider->name(), QStringLiteral("Searching"));
    emit statusChanged(QStringLiteral("%1 — searching: %2")
                           .arg(provider->name(), query));

    // The scraped endpoint gets a jittered gap; the JSON APIs do not need one.
    // The first query is immediate so Start still feels responsive.
    const int delayMs = firstQuery_
        ? 0
        : (next.media ? 150
                      : rateGapMs_ + QRandomGenerator::global()->bounded(0, 500));

    firstQuery_ = false;

    if (delayMs == 0) {
        provider->search(query);
        return;
    }

    pendingDispatch_ = true;

    QTimer::singleShot(delayMs, this, [this, provider, query]() {
        pendingDispatch_ = false;

        if (state_ != State::Searching || stopRequested_) {
            return;
        }

        provider->search(query);
    });
}

void ResearchEngine::finishResearch()
{
    if (state_ == State::Idle) {
        return;
    }

    state_ = State::Idle;

    if (!generalAvailable_ && resultCount_ == 0) {
        emit statusChanged(QStringLiteral("Research provider unavailable. "
                                          "%1 result(s) retained.")
                               .arg(resultCount_));
    } else {
        emit providerStatusChanged(general_->name(), QStringLiteral("Ready"));

        emit statusChanged(QStringLiteral("Research complete — %1 result(s) found.")
                               .arg(resultCount_));
    }

    emit researchStopped();
}

} // namespace efcrawler
