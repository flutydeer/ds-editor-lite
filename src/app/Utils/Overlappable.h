//
// Created by fluty on 2024/2/1.
//

#ifndef IOVERLAPABLE_H
#define IOVERLAPABLE_H

#include <QtGlobal>
#include <tuple>

class Overlappable {
public:
    virtual ~Overlappable() = default;
    [[nodiscard]] bool overlapped() const {
        return m_overlappedCounter;
    }
    void acquireOverlappedCounter() {
        m_overlappedCounter++;
    }
    void releaseOverlappedCounter() {
        Q_ASSERT(m_overlappedCounter > 0);
        m_overlappedCounter--;
    }
    void clearOverlappedCounter() {
        m_overlappedCounter = 0;
    }
    [[nodiscard]] virtual std::tuple<qsizetype, qsizetype> interval() const = 0;

private:
    int m_overlappedCounter = 0;
};

#endif // IOVERLAPABLE_H
