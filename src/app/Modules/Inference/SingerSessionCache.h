#ifndef SINGERSESSIONCACHE_H
#define SINGERSESSIONCACHE_H

#include <memory>
#include <mutex>
#include <utility>

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

#include <QHash>

template <typename Handle>
class SingerSessionCache final {
public:
    template <typename Factory>
    std::shared_ptr<Handle> acquire(const SingerIdentifier &identifier, Factory &&factory) {
        std::shared_ptr<Handle> displaced;
        std::shared_ptr<Handle> result;
        {
            std::lock_guard lock(m_mutex);
            if (auto it = m_handles.find(identifier); it != m_handles.end()) {
                if (!it.value()->isStale()) {
                    return it.value();
                }
                displaced = std::move(it.value());
                m_handles.erase(it);
            }

            result = std::forward<Factory>(factory)();
            if (result) {
                m_handles.insert(identifier, result);
            }
        }
        return result;
    }

    void clear() {
        QHash<SingerIdentifier, std::shared_ptr<Handle>> released;
        {
            std::lock_guard lock(m_mutex);
            m_handles.swap(released);
        }
    }

private:
    std::mutex m_mutex;
    QHash<SingerIdentifier, std::shared_ptr<Handle>> m_handles;
};

#endif // SINGERSESSIONCACHE_H
