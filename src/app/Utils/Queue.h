//
// Created by fluty on 24-9-5.
//

#ifndef QUEUE_H
#define QUEUE_H

#include <QList>

template <typename T>
class Queue {
public:
    // Queue methods
    void enqueue(const T &item) {
        m_list.append(item);
    }

    [[nodiscard]] T dequeue() {
        return m_list.takeFirst();
    }

    T &head() {
        return m_list.first();
    }

    const T &head() const {
        return m_list.first();
    }

    T &tail() {
        return m_list.last();
    }

    const T &tail() const {
        return m_list.last();
    }

    // List methods
    bool remove(const T &item) {
        return m_list.removeOne(item);
    }

    template <typename TPredicate>
    bool removeIf(const TPredicate &predicate) {
        return m_list.removeIf(predicate);
    }

    void clear() {
        m_list.clear();
    }

    QList<T> toList() {
        return m_list;
    }

    const QList<T> &toList() const {
        return m_list;
    }

    [[nodiscard]] int count() const {
        return m_list.count();
    }

    // Iterators
    using iterator = typename QList<T>::const_iterator;
    using const_iterator = typename QList<T>::const_iterator;
    using reverse_iterator = typename QList<T>::const_reverse_iterator;
    using const_reverse_iterator = typename QList<T>::const_reverse_iterator;

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
    QList<T> m_list;
};

#endif // QUEUE_H
