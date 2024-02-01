//
// Created by fluty on 2024/2/1.
//

#ifndef SERIALLIST_H
#define SERIALLIST_H

#include <QList>

#include "IOverlapable.h"

template <typename T>
class OverlapableSerialList {
public:
    int count() const;
    void add(IOverlapable *item);
    void remove(IOverlapable *item);
    void clear();
    int indexOf(const IOverlapable *item);
    bool contains(const IOverlapable *item);
    T *at(int index) const;
    QList<T *> items() const;
    bool isOverlappedItemExists() const;
    QList<T *> findOverlappedItems(IOverlapable *obj) const;
    QList<T *> overlappedItems() const;

private:
    QList<IOverlapable *> m_list;
};
template <typename T>
int OverlapableSerialList<T>::count() const {
    return m_list.size();
}
template <typename T>
void OverlapableSerialList<T>::add(IOverlapable *item) {
    item->setOverlapped(false);
    if (m_list.empty()) {
        m_list.append(item);
        return;
    }
    int i;
    for (i = 0; i < m_list.size(); ++i) {
        auto other = m_list.at(i);
        if (other->isOverlappedWith(item)) {
            other->setOverlapped(true);
            item->setOverlapped(true);
        }
        if (item->compareTo(other) < 0) {
            break;
        }
    }
    m_list.insert(i, item);
}
template <typename T>
void OverlapableSerialList<T>::remove(IOverlapable *item) {
    m_list.removeOne(item);
}
template <typename T>
void OverlapableSerialList<T>::clear() {
    m_list.clear();
}
template <typename T>
int OverlapableSerialList<T>::indexOf(const IOverlapable *item) {
    return m_list.indexOf(item);
}
template <typename T>
bool OverlapableSerialList<T>::contains(const IOverlapable *item) {
    return m_list.contains(item);
}
template <typename T>
T *OverlapableSerialList<T>::at(int index) const {
    auto obj = m_list.at(index);
    return static_cast<T *>(obj);
}
template <typename T>
QList<T *> OverlapableSerialList<T>::items() const {
    auto list = new QList<T *>;
    for (const auto &item : m_list)
        list->append(static_cast<T *>(item));
    return *list;
}
template <typename T>
bool OverlapableSerialList<T>::isOverlappedItemExists() const {
    bool overlapped = false;
    for (const auto &item : m_list)
        if (item->overlapped()) {
            overlapped = true;
            break;
        }
    return overlapped;
}
template <typename T>
QList<T *> OverlapableSerialList<T>::findOverlappedItems(IOverlapable *obj) const {
    auto list = new QList<T *>;
    for (const auto &item : m_list)
        if (item->isOverlappedWith(obj))
            list->append(static_cast<T *>(item));
    return *list;
}
template <typename T>
QList<T *> OverlapableSerialList<T>::overlappedItems() const {
    auto list = new QList<T *>;
    for (const auto &item : m_list)
        if (item->overlapped())
            list->append(static_cast<T *>(item));
    return *list;
}

#endif // SERIALLIST_H
