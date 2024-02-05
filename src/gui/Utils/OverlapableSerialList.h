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
    void update(IOverlapable *item);
    void clear();
    int indexOf(const IOverlapable *item);
    bool contains(const IOverlapable *item);
    T *at(int index) const;
    bool isOverlappedItemExists() const;
    QList<T *> findOverlappedItems(IOverlapable *obj) const;
    QList<T *> overlappedItems() const;

    class iterator;
    class const_iterator;

    class iterator {
    public:
        using value_type = T *;
        using pointer = T **;
        using reference = T *&;
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = qptrdiff;

        inline constexpr iterator() = default;
        inline reference operator*() const { return reinterpret_cast<reference>(*i); }
        inline pointer operator->() const { return reinterpret_cast<pointer>(&*i); }
        inline reference operator[](difference_type j) const { return reinterpret_cast<reference>(*(i + j)); }
        inline constexpr bool operator==(iterator o) const { return i == o.i; }
        inline constexpr bool operator!=(iterator o) const { return i != o.i; }
        inline constexpr bool operator<(iterator other) const { return i < other.i; }
        inline constexpr bool operator<=(iterator other) const { return i <= other.i; }
        inline constexpr bool operator>(iterator other) const { return i > other.i; }
        inline constexpr bool operator>=(iterator other) const { return i >= other.i; }
        inline constexpr bool operator==(const_iterator o) const { return i == o.i; }
        inline constexpr bool operator!=(const_iterator o) const { return i != o.i; }
        inline constexpr bool operator<(const_iterator other) const { return i < other.i; }
        inline constexpr bool operator<=(const_iterator other) const { return i <= other.i; }
        inline constexpr bool operator>(const_iterator other) const { return i > other.i; }
        inline constexpr bool operator>=(const_iterator other) const { return i >= other.i; }
        inline constexpr bool operator==(pointer p) const { return i == p; }
        inline constexpr bool operator!=(pointer p) const { return i != p; }
        inline iterator &operator++() { ++i; return *this; }
        inline iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        inline iterator &operator--() { --i; return *this; }
        inline iterator operator--(int) { auto copy = *this; --*this; return copy; }
        inline difference_type operator-(iterator j) const { return i - j.i; }
        inline iterator &operator+=(difference_type j) { i += j; return *this; }
        inline iterator &operator-=(difference_type j) { i -= j; return *this; }
        inline iterator operator+(difference_type j) const { return iterator(i + j); }
        inline iterator operator-(difference_type j) const { return iterator(i - j); }
        friend inline iterator operator+(difference_type j, iterator k) { return k + j; }

    private:
        friend class OverlapableSerialList<T>;
        explicit iterator(QList<IOverlapable *>::iterator i_) : i(i_) {}
        QList<IOverlapable *>::iterator i;
    };

    class const_iterator {
    public:
        using value_type = T *;
        using pointer = T * const *;
        using reference = T * const &;
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = qptrdiff;

        inline constexpr const_iterator() = default;
        inline constexpr const_iterator(iterator o): i(o.i) {}
        inline reference operator*() const { return reinterpret_cast<reference>(*i); }
        inline pointer operator->() const { return reinterpret_cast<pointer>(&*i); }
        inline reference operator[](qsizetype j) const { return reinterpret_cast<reference>(*(i + j)); }
        inline constexpr bool operator==(const_iterator o) const { return i == o.i; }
        inline constexpr bool operator!=(const_iterator o) const { return i != o.i; }
        inline constexpr bool operator<(const_iterator other) const { return i < other.i; }
        inline constexpr bool operator<=(const_iterator other) const { return i <= other.i; }
        inline constexpr bool operator>(const_iterator other) const { return i > other.i; }
        inline constexpr bool operator>=(const_iterator other) const { return i >= other.i; }
        inline constexpr bool operator==(iterator o) const { return i == o.i; }
        inline constexpr bool operator!=(iterator o) const { return i != o.i; }
        inline constexpr bool operator<(iterator other) const { return i < other.i; }
        inline constexpr bool operator<=(iterator other) const { return i <= other.i; }
        inline constexpr bool operator>(iterator other) const { return i > other.i; }
        inline constexpr bool operator>=(iterator other) const { return i >= other.i; }
        inline constexpr bool operator==(pointer p) const { return i == p; }
        inline constexpr bool operator!=(pointer p) const { return i != p; }
        inline const_iterator &operator++() { ++i; return *this; }
        inline const_iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        inline const_iterator &operator--() { --i; return *this; }
        inline const_iterator operator--(int) { auto copy = *this; --*this; return copy; }
        inline difference_type operator-(const_iterator j) const { return i - j.i; }
        inline const_iterator &operator+=(difference_type j) { i += j; return *this; }
        inline const_iterator &operator-=(difference_type j) { i -= j; return *this; }
        inline const_iterator operator+(difference_type j) const { return const_iterator(i + j); }
        inline const_iterator operator-(difference_type j) const { return const_iterator(i - j); }
        friend inline const_iterator operator+(difference_type j, const_iterator k) { return k + j; }

    private:
        friend class OverlapableSerialList<T>;
        explicit const_iterator(QList<IOverlapable *>::const_iterator i_) : i(i_) {}
        QList<IOverlapable *>::const_iterator i;
    };

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() { return iterator(m_list.begin()); }
    iterator end() { return iterator(m_list.end()); }
    const_iterator begin() const { return const_iterator(m_list.begin()); }
    const_iterator end() const { return const_iterator(m_list.end()); }
    const_iterator cbegin() const { return const_iterator(m_list.cbegin()); }
    const_iterator cend() const { return const_iterator(m_list.cend()); }
    reverse_iterator rbegin() { return reverse_iterator(m_list.rbegin()); }
    reverse_iterator rend() { return reverse_iterator(m_list.rend()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(m_list.rbegin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(m_list.rend()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(m_list.crbegin()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(m_list.crend()); }

private:
    QList<IOverlapable *> &overlappedIOverlapables();
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
    if (item->overlapped()) {
        item->setOverlapped(false);
        auto overlapedItemsInside = this->overlappedIOverlapables();
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
template <typename T>
void OverlapableSerialList<T>::update(IOverlapable *item) {
    remove(item);
    add(item);
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
template <typename T>
QList<IOverlapable *> &OverlapableSerialList<T>::overlappedIOverlapables() {
    auto list = new QList<IOverlapable *>;
    for (const auto &item : m_list)
        if (item->overlapped())
            list->append(item);
    return *list;
}

#endif // SERIALLIST_H
