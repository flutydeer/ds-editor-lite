#include "SingletonRegistry.h"

#include <unordered_map>
#include <mutex>

struct SingletonRegistry::Services {
    std::mutex mutex;
    std::unordered_map<const void *, void *> objects;
};

namespace {
    thread_local SingletonRegistry::Context currentContext;
}

SingletonRegistry::Context SingletonRegistry::createContext() {
    return std::make_shared<Services>();
}

SingletonRegistry::Context SingletonRegistry::current() {
    static const auto process = createContext();
    return currentContext ? currentContext : process;
}

SingletonRegistry::Scope::Scope(const Context &context) : m_previous(currentContext) {
    if (context)
        currentContext = context;
}

SingletonRegistry::Scope::~Scope() {
    currentContext = std::move(m_previous);
}

void *SingletonRegistry::get(const void *key) {
    auto &state = *current();
    const std::lock_guard lock(state.mutex);
    const auto &t = state.objects;
    const auto it = t.find(key);
    return it == t.end() ? nullptr : it->second;
}

void SingletonRegistry::set(const void *key, void *value) {
    auto &state = *current();
    const std::lock_guard lock(state.mutex);
    state.objects[key] = value;
}

void SingletonRegistry::clear() {
    auto &state = *current();
    const std::lock_guard lock(state.mutex);
    state.objects.clear();
}
