#include "SingletonRegistry.h"

#include <unordered_map>

namespace {
    std::unordered_map<const void *, void *> &table() {
        static std::unordered_map<const void *, void *> t;
        return t;
    }
}

void *SingletonRegistry::get(const void *key) {
    const auto &t = table();
    const auto it = t.find(key);
    return it == t.end() ? nullptr : it->second;
}

void SingletonRegistry::set(const void *key, void *value) {
    table()[key] = value;
}

void SingletonRegistry::clear() {
    table().clear();
}
