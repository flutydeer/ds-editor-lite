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
        // Destroy displaced models after releasing the cache mutex.
        std::vector<std::shared_ptr<Handle>> displaced;
        std::size_t generation = 0;

        // Reuse only live, nonstale leases; only retained singers receive resident references.
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
            generation = m_generation;
        }

        // Build outside the mutex so model creation cannot block cache maintenance or shutdown.
        // Concurrent callers share the first valid lease inserted.
        auto result = std::forward<Factory>(factory)();
        if (!result) {
            return result;
        }

        {
            std::lock_guard lock(m_mutex);
            // Do not reinsert a lease invalidated while its factory ran.
            if (generation != m_generation || result->isStale())
                return result;
            if (auto existing = reuse(identifier)) {
                return existing;
            }
            // Keep weak entries for active callers; only retained singers hold resident references.
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
                // Keep active weak entries available for reuse.
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

    HandleList clear(bool clearRetention = true) {
        HandleList handles;
        {
            std::lock_guard lock(m_mutex);
            if (clearRetention)
                m_retainedIdentifiers.clear();
            handles.reserve(static_cast<std::size_t>(m_entries.size()));
            for (auto &entry : m_entries) {
                if (entry.resident) {
                    handles.push_back(std::move(entry.resident));
                }
            }
            m_entries.clear();
            ++m_generation;
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
    std::size_t m_generation = 0;
};

#endif // SINGERSESSIONCACHE_H
