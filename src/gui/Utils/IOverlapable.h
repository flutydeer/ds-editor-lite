//
// Created by fluty on 2024/2/1.
//

#ifndef IOVERLAPABLE_H
#define IOVERLAPABLE_H

class IOverlapable {
public:
    bool overlapped() const {
        return m_overlapped;
    }
    void setOverlapped(bool b);

protected:
    bool m_overlapped = false;
};
inline void IOverlapable::setOverlapped(bool b) {
    m_overlapped = b;
}

#endif // IOVERLAPABLE_H
