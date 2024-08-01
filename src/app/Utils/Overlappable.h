//
// Created by fluty on 2024/2/1.
//

#ifndef IOVERLAPABLE_H
#define IOVERLAPABLE_H

#include <qtypes.h>
#include <tuple>

class Overlappable {
public:
    virtual ~Overlappable() = default;
    [[nodiscard]] bool overlapped() const {
        return m_overlapped;
    }
    void setOverlapped(bool b) {
        m_overlapped = b;
    }
    [[nodiscard]] virtual std::tuple<qsizetype, qsizetype> interval() const = 0;

private:
    bool m_overlapped = false;
};

#endif // IOVERLAPABLE_H
