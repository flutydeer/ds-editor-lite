//
// Created by fluty on 2024/2/3.
//

#ifndef IDGENERATOR_H
#define IDGENERATOR_H

#include <atomic>

#include "Singleton.h"

class IdGenerator : public Singleton<IdGenerator> {
public:
    IdGenerator() : Singleton<IdGenerator>(), m_id(0) {
    }
    ~IdGenerator() noexcept override = default;

    int next() noexcept {
        return m_id.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::atomic<int> m_id;
};

#endif // IDGENERATOR_H
