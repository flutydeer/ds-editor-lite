//
// Created by fluty on 2024/2/1.
//

#ifndef SERIALLIST_H
#define SERIALLIST_H

#include <set>
#include <unordered_set>

#include <interval-tree/interval_tree.hpp>

#include <QList>

template <typename T>
class OverlappableSerialList {
    struct Interval : lib_interval_tree::interval<qsizetype, lib_interval_tree::right_open> {
        T *item;

        Interval(const std::tuple<qsizetype, qsizetype> &range, T *item)
            : lib_interval_tree::interval<qsizetype, lib_interval_tree::right_open>(
                  std::get<0>(range), std::max(std::get<0>(range), std::get<1>(range) - 1)),
              item(item) {
        }
    };

    struct ItemCmp {
        bool operator()(const T *a, const T *b) const {
            if (a->compareTo(b) == 0)
                return a < b;
            return a->compareTo(b) < 0;
        }
    };

public:
    int count() const;
    void add(T *item);
    void remove(T *item);
    void clear();
    bool contains(const T *item);
    bool hasOverlappedItem() const;
    QList<T *> findOverlappedItems(T *obj) const;
    QList<T *> findOverlappedItems(const std::tuple<qsizetype, qsizetype> &interval_) const;
    QList<T *> overlappedItems() const;
    QList<T *> toList() const;

    using iterator = std::set<T *, ItemCmp>::const_iterator;
    using const_iterator = std::set<T *, ItemCmp>::const_iterator;
    using reverse_iterator = std::set<T *, ItemCmp>::const_reverse_iterator;
    using const_reverse_iterator = std::set<T *, ItemCmp>::const_reverse_iterator;

    iterator begin() {
        return m_items.cbegin();
    }

    iterator end() {
        return m_items.cend();
    }

    const_iterator begin() const {
        return m_items.cbegin();
    }

    const_iterator end() const {
        return m_items.cend();
    }

    const_iterator cbegin() const {
        return m_items.cbegin();
    }

    const_iterator cend() const {
        return m_items.cend();
    }

    reverse_iterator rbegin() {
        return m_items.crbegin();
    }

    reverse_iterator rend() {
        return m_items.crend();
    }

    const_reverse_iterator rbegin() const {
        return m_items.crbegin();
    }

    const_reverse_iterator rend() const {
        return m_items.crend();
    }

    const_reverse_iterator crbegin() const {
        return m_items.crbegin();
    }

    const_reverse_iterator crend() const {
        return m_items.crend();
    }

private:
    lib_interval_tree::interval_tree<Interval> m_intervalTree;
    std::set<T *, ItemCmp> m_items;
    std::unordered_set<T *> m_itemHash;
    int m_overlappedCounter{};
};

template <typename T>
int OverlappableSerialList<T>::count() const {
    return m_intervalTree.size();
}

template <typename T>
void OverlappableSerialList<T>::add(T *item) {
    item->clearOverlappedCounter();
    Interval interval(item->interval(), item);
    m_intervalTree.overlap_find_all(interval, [this, item](auto it) {
        if (!it.interval().item->overlapped())
            m_overlappedCounter++;
        it.interval().item->acquireOverlappedCounter();
        item->acquireOverlappedCounter();
        return true;
    });
    if (item->overlapped())
        m_overlappedCounter++;
    m_intervalTree.insert(interval);
    m_items.insert(item);
    m_itemHash.insert(item);
}

template <typename T>
void OverlappableSerialList<T>::remove(T *item) {
    if (item->overlapped())
        m_overlappedCounter--;
    item->clearOverlappedCounter();
    Interval interval(item->interval(), item);
    auto itToErase = m_intervalTree.end();
    m_intervalTree.overlap_find_all(interval, [this, item, &itToErase](auto it) {
        if (it.interval().item == item)
            itToErase = it;
        else {
            it.interval().item->releaseOverlappedCounter();
            if (!it.interval().item->overlapped())
                m_overlappedCounter--;
        }
        return true;
    });
    // qDebug() << "erase" << item;
    m_intervalTree.erase(itToErase);
    m_items.erase(item);
    m_itemHash.erase(item);
}

template <typename T>
void OverlappableSerialList<T>::clear() {
    m_intervalTree.clear();
    m_overlappedCounter = 0;
}

template <typename T>
bool OverlappableSerialList<T>::contains(const T *item) {
    return m_itemHash.count(item);
}

template <typename T>
bool OverlappableSerialList<T>::hasOverlappedItem() const {
    return m_overlappedCounter;
}

template <typename T>
QList<T *> OverlappableSerialList<T>::findOverlappedItems(T *obj) const {
    return findOverlappedItems(obj->interval());
}

template <typename T>
QList<T *> OverlappableSerialList<T>::findOverlappedItems(
    const std::tuple<qsizetype, qsizetype> &interval_) const {
    auto interval = Interval(interval_, nullptr);
    QList<T *> ret;
    m_intervalTree.overlap_find_all(interval, [&ret](auto it) {
        ret.append(it->interval().item);
        return true;
    });
    return ret;
}

template <typename T>
QList<T *> OverlappableSerialList<T>::overlappedItems() const {
    QList<T *> ret;
    std::copy_if(m_itemHash.cbegin(), m_itemHash.cend(), std::back_inserter(ret),
                 [](auto item) { return item->overlapped(); });
    return ret;
}

template <typename T>
QList<T *> OverlappableSerialList<T>::toList() const {
    QList<T *> result;
    for (const auto &item : m_items)
        result.append(item);
    return result;
}

#endif // SERIALLIST_H
