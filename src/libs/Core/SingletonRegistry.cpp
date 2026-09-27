#include "SingletonRegistry.h"

#include <unordered_map>
#include <mutex>

namespace {
    struct Registry {
        std::mutex mutex;
        std::unordered_map<const void *, void *> objects;
    };

    Registry &registry() {
        static Registry instance;
        return instance;
    }
}

void *SingletonRegistry::get(const void *key) {
    auto &state = registry();
    const std::lock_guard lock(state.mutex);
    const auto &t = state.objects;
    const auto it = t.find(key);
    return it == t.end() ? nullptr : it->second;
}

void SingletonRegistry::set(const void *key, void *value) {
    auto &state = registry();
    const std::lock_guard lock(state.mutex);
    state.objects[key] = value;
}

void SingletonRegistry::clear() {
    auto &state = registry();
    const std::lock_guard lock(state.mutex);
    state.objects.clear();
}
