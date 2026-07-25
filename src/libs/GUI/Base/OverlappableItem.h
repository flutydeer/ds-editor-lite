//
// Created by fluty on 24-8-1.
//

#ifndef OVERLAPPABLEITEM_H
#define OVERLAPPABLEITEM_H

class OverlappableItem {
public:
    [[nodiscard]] bool overlapped() const {
        return m_overlapped;
    }

    void setOverlapped(bool b) {
        m_overlapped = b;
    }

private:
    bool m_overlapped = false;
};

#endif // OVERLAPPABLEITEM_H
