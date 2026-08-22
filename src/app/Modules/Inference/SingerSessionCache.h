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
        const auto now = Clock::now();
        std::shared_ptr<Handle> displaced;
        std::shared_ptr<Handle> result;
        {
            std::lock_guard lock(m_mutex);
            if (auto it = m_entries.find(identifier); it != m_entries.end()) {
                auto existing = it->resident ? it->resident : it->live.lock();
                if (existing && !existing->isStale()) {
                    it->resident = existing;
                    it->lastUsed = now;
                    return existing;
                }
                displaced = std::move(it->resident);
                m_entries.erase(it);
            }

            result = std::forward<Factory>(factory)();
            if (result && m_retainedIdentifiers.contains(identifier)) {
                m_entries.insert(identifier, Entry{result, result, now});
            }
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
                it = m_entries.erase(it);
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
