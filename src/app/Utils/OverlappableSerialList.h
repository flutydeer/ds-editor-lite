//
// Created by fluty on 2024/2/1.
//

#ifndef SERIALLIST_H
#define SERIALLIST_H

#include <QList>

template <typename T>
class OverlappableSerialList {
public:
    [[nodiscard]] int count() const;
    void add(T *item);
    void remove(T *item);
    void clear();
    int indexOf(const T *item);
    bool contains(const T *item);
    T *at(int index) const;
    [[nodiscard]] bool hasOverlappedItem() const;
    QList<T *> findOverlappedItems(T *obj) const;
    QList<T *> overlappedItems() const;
    QList<T *> toList() const;

    using iterator = typename QList<T *>::const_iterator;
    using const_iterator = typename QList<T *>::const_iterator;
    using reverse_iterator = typename QList<T *>::const_reverse_iterator;
    using const_reverse_iterator = typename QList<T *>::const_reverse_iterator;

    iterator begin() {
        return m_list.cbegin();
    }
    iterator end() {
        return m_list.cend();
    }
    const_iterator begin() const {
        return m_list.cbegin();
    }
    const_iterator end() const {
        return m_list.cend();
    }
    const_iterator cbegin() const {
        return m_list.cbegin();
    }
    const_iterator cend() const {
        return m_list.cend();
    }
    reverse_iterator rbegin() {
        return m_list.crbegin();
    }
    reverse_iterator rend() {
        return m_list.crend();
    }
    const_reverse_iterator rbegin() const {
        return m_list.crbegin();
    }
    const_reverse_iterator rend() const {
        return m_list.crend();
    }
    const_reverse_iterator crbegin() const {
        return m_list.crbegin();
    }
    const_reverse_iterator crend() const {
        return m_list.crend();
    }

private:
    QList<T *> overlappedOverlappables() const;
    QList<T *> m_list;
};
template <typename T>
int OverlappableSerialList<T>::count() const {
    return m_list.size();
}
template <typename T>
void OverlappableSerialList<T>::add(T *item) {
    item->setOverlapped(false);
    if (m_list.empty()) {
        m_list.append(item);
        return;
    }
    int num = -1;
    for (int i = 0; i < m_list.size(); i++) {
        auto t = m_list.at(i);
        if (num == -1 && t->compareTo(item) > 0)
            num = i;
        if (num == -1) {
            if (t->isOverlappedWith(item)) {
                t->setOverlapped(true);
                item->setOverlapped(true);
            }
        } else {
            if (!t->isOverlappedWith(item))
                break;
            t->setOverlapped(true);
            item->setOverlapped(true);
        }
    }
    if (num != -1) {
        m_list.insert(num, item);
        return;
    }
    m_list.append(item);
}
template <typename T>
void OverlappableSerialList<T>::remove(T *item) {
    if (item->overlapped()) {
        item->setOverlapped(false);
        auto overlapedItemsInside = this->overlappedItems();
        for (auto t : overlapedItemsInside) {
            if (t->isOverlappedWith(item)) {
                bool flag = false;
                for (auto t2 : overlapedItemsInside) {
                    if (t2 != t && t2->isOverlappedWith(t)) {
                        flag = true;
                        break;
                    }
                }
                t->setOverlapped(flag);
            }
        }
    }
    m_list.removeOne(item);
}
// template <typename T>
// void OverlapableSerialList<T>::update(T *item) {
//     remove(item);
//     add(item);
// }
template <typename T>
void OverlappableSerialList<T>::clear() {
    m_list.clear();
}
template <typename T>
int OverlappableSerialList<T>::indexOf(const T *item) {
    return m_list.indexOf(item);
}
template <typename T>
bool OverlappableSerialList<T>::contains(const T *item) {
    return m_list.contains(item);
}
template <typename T>
T *OverlappableSerialList<T>::at(int index) const {
    return m_list.at(index);
}
template <typename T>
bool OverlappableSerialList<T>::hasOverlappedItem() const {
    bool overlapped = false;
    for (const auto &item : m_list)
        if (item->overlapped()) {
            overlapped = true;
            break;
        }
    return overlapped;
}
template <typename T>
QList<T *> OverlappableSerialList<T>::findOverlappedItems(T *obj) const {
    QList<T *> list;
    for (const auto &item : m_list)
        if (item->isOverlappedWith(obj))
            list.append(item);
    return list;
}
template <typename T>
QList<T *> OverlappableSerialList<T>::overlappedItems() const {
    QList<T *> list;
    for (const auto &item : m_list)
        if (item->overlapped())
            list.append(item);
    return list;
}
template <typename T>
QList<T *> OverlappableSerialList<T>::toList() const {
    return m_list;
}
template <typename T>
QList<T *> OverlappableSerialList<T>::overlappedOverlappables() const {
    QList<T *> list;
    for (const auto &item : m_list)
        if (item->overlapped())
            list.append(item);
    return list;
}

#endif // SERIALLIST_H
