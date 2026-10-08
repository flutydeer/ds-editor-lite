#ifndef SINGERSESSIONCACHE_H
#define SINGERSESSIONCACHE_H

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

#include <QHash>
#include <QSet>

template <typename Handle, typename Clock = std::chrono::steady_clock>
class SingerSessionCache final {
public:
    using Duration = typename Clock::duration;
    using HandleList = std::vector<std::shared_ptr<Handle>>;

    struct RetentionResult {
        std::size_t released = 0;
        HandleList handles;
    };

    struct EvictionResult {
        std::size_t idle = 0;
        std::size_t capacity = 0;
        HandleList handles;
    };

    template <typename Factory>
    std::shared_ptr<Handle> acquire(const SingerIdentifier &identifier, Factory &&factory) {
        // This vector outlives both locked sections, so that a displaced handle is destroyed after
        // the mutex is released. Closing the models that the handle owns must not happen under a
        // lock on which every other caller of this cache waits.
        std::vector<std::shared_ptr<Handle>> displaced;

        // Returns the handle of a live entry and is called under the lock by both passes below.
        // Reuse updates the timestamp, so that a second caller shares the handle of the first caller
        // rather than building a new handle. Only a retained singer is promoted to resident, which is
        // the same rule that the insertion path below applies. A stale entry is removed, and its
        // resident handle is moved to `displaced`.
        const auto reuse = [this, &displaced](const SingerIdentifier &wanted) {
            std::shared_ptr<Handle> found;
            if (auto it = m_entries.find(wanted); it != m_entries.end()) {
                auto existing = it->resident ? it->resident : it->live.lock();
                if (existing && !existing->isStale()) {
                    it->resident = m_retainedIdentifiers.contains(wanted) ? existing : nullptr;
                    it->lastUsed = Clock::now();
                    return existing;
                }
                displaced.push_back(std::move(it->resident));
                m_entries.erase(it);
            }
            return found;
        };

        {
            std::lock_guard lock(m_mutex);
            if (auto existing = reuse(identifier)) {
                return existing;
            }
        }

        // The handle is built without holding the mutex. A handle opens the five models of the
        // singer, which takes seconds. A build under the lock would block the retention sweep, the
        // eviction timer and the shutdown that clears the cache, and the shutdown runs on the GUI
        // thread at exit.
        //
        // Two callers may therefore build concurrently. The handle of the second caller to finish
        // is dropped below, and both callers continue with the first handle, which preserves one
        // lease per singer. No expensive object is built twice, because the engine returns the same
        // pipeline to every caller for a given singer and the handle is a lease over that pipeline.
        auto result = std::forward<Factory>(factory)();
        if (!result) {
            return result;
        }

        {
            std::lock_guard lock(m_mutex);
            if (auto existing = reuse(identifier)) {
                return existing;
            }
            // The entry is recorded whether or not the singer is retained. Two callers that request
            // a singer without retention must still share one lease, because the pipeline behind
            // the lease is a single object and the destruction of the first lease would release it
            // from the other caller. Only a retained singer receives a resident reference that
            // outlives its callers.
            const bool retained = m_retainedIdentifiers.contains(identifier);
            m_entries.insert(identifier,
                             Entry{retained ? result : nullptr, result, Clock::now()});
        }
        return result;
    }

    RetentionResult retainOnly(const QSet<SingerIdentifier> &identifiers) {
        RetentionResult result;
        {
            std::lock_guard lock(m_mutex);
            m_retainedIdentifiers = identifiers;
            for (auto it = m_entries.begin(); it != m_entries.end();) {
                if (identifiers.contains(it.key())) {
                    ++it;
                    continue;
                }
                if (it->resident) {
                    result.handles.push_back(std::move(it->resident));
                    ++result.released;
                }
                // An entry whose lease is still held remains in the map, so that the next caller
                // shares the lease instead of building a second pipeline for the same singer.
                if (it->live.expired()) {
                    it = m_entries.erase(it);
                } else {
                    ++it;
                }
            }
        }
        return result;
    }

    QSet<SingerIdentifier> retainedIdentifiers() const {
        std::lock_guard lock(m_mutex);
        return m_retainedIdentifiers;
    }

    EvictionResult evict(const std::size_t capacity, const Duration idleTimeout) {
        const auto now = Clock::now();
        EvictionResult result;
        {
            std::lock_guard lock(m_mutex);
            for (auto it = m_entries.begin(); it != m_entries.end();) {
                if (!it->resident) {
                    if (it->live.expired()) {
                        it = m_entries.erase(it);
                    } else {
                        ++it;
                    }
                    continue;
                }
                if (idleTimeout > Duration::zero() && now - it->lastUsed >= idleTimeout) {
                    result.handles.push_back(std::move(it->resident));
                    ++result.idle;
                }
                ++it;
            }

            std::vector<Entry *> residents;
            residents.reserve(static_cast<std::size_t>(m_entries.size()));
            for (auto &entry : m_entries) {
                if (entry.resident) {
                    residents.push_back(&entry);
                }
            }
            if (capacity > 0 && residents.size() > capacity) {
                std::sort(residents.begin(), residents.end(),
                          [](const Entry *left, const Entry *right) {
                              return left->lastUsed < right->lastUsed;
                          });
                const auto count = residents.size() - capacity;
                result.handles.reserve(result.handles.size() + count);
                for (std::size_t i = 0; i < count; ++i) {
                    result.handles.push_back(std::move(residents[i]->resident));
                    ++result.capacity;
                }
            }
        }
        return result;
    }

    HandleList clear() {
        HandleList handles;
        {
            std::lock_guard lock(m_mutex);
            m_retainedIdentifiers.clear();
            handles.reserve(static_cast<std::size_t>(m_entries.size()));
            for (auto &entry : m_entries) {
                if (entry.resident) {
                    handles.push_back(std::move(entry.resident));
                }
            }
            m_entries.clear();
        }
        return handles;
    }

private:
    struct Entry {
        std::shared_ptr<Handle> resident;
        std::weak_ptr<Handle> live;
        typename Clock::time_point lastUsed;
    };

    mutable std::mutex m_mutex;
    QSet<SingerIdentifier> m_retainedIdentifiers;
    QHash<SingerIdentifier, Entry> m_entries;
};

#endif // SINGERSESSIONCACHE_H
