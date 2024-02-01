//
// Created by fluty on 2024/2/1.
//

#ifndef SERIALLIST_H
#define SERIALLIST_H

#include <QList>

#include "IComparable.h"

template <typename T>
class SerialList {
public:
    int count() const;
    void add(IComparable *item);
    void remove(IComparable *item);
    void clear();
    int indexOf(const IComparable *item);
    bool contains(const IComparable *item);
    T *at(int index) const;
    // const QList<IComparable *> &items() const;

private:
    QList<IComparable *> m_list;
};
template <typename T>
int SerialList<T>::count() const {
    return m_list.count();
}
template <typename T>
void SerialList<T>::add(IComparable *item) {
    if (m_list.count() == 0) {
        m_list.append(item);
        return;
    }
    int i;
    for (i = 0; i < m_list.count(); ++i) {
        auto other = m_list.at(i);
        if (item->compareTo(other) < 0) {
            break;
        }
    }
    m_list.insert(i, item);
}
template <typename T>
void SerialList<T>::remove(IComparable *item) {
    m_list.removeOne(item);
}
template <typename T>
void SerialList<T>::clear() {
    m_list.clear();
}
template <typename T>
int SerialList<T>::indexOf(const IComparable *item) {
    return m_list.indexOf(item);
}
template <typename T>
bool SerialList<T>::contains(const IComparable *item) {
    return m_list.contains(item);
}
template <typename T>
T *SerialList<T>::at(int index) const {
    auto obj = m_list.at(index);
    return static_cast<T *>(obj);
}
// template <typename T>
// const QList<IComparable *> &SerialList<T>::items() const {
//     return m_list;
// }

#endif // SERIALLIST_H
