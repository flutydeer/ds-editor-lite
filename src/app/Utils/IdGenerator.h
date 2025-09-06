//
// Created by fluty on 2024/2/3.
//

#ifndef IDGENERATOR_H
#define IDGENERATOR_H

#include <atomic>

#include "Singleton.h"

class IdGenerator {
private:
    IdGenerator() : m_id(0) {
    }
    ~IdGenerator() = default;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(IdGenerator)

    IdGenerator(const IdGenerator &) = delete;
    IdGenerator &operator=(const IdGenerator &) = delete;
    IdGenerator(IdGenerator &&) = delete;
    IdGenerator &operator=(IdGenerator &&) = delete;

public:
    int next() noexcept {
        return m_id.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::atomic<int> m_id;
};

#endif // IDGENERATOR_H
