//
// Created by fluty on 2024/2/6.
//

#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <QPoint>

class MathUtils {
public:
    static double clip(double value, double min, double max) {
        if (value < min)
            return min;
        if (value > max)
            return max;
        return value;
    }
    static int round(int tick, int step) {
        int times = tick / step;
        int mod = tick % step;
        if (mod > step / 2)
            return step * (times + 1);
        return step * times;
    }

    static int roundDown(int tick, int step) {
        int times = tick / step;
        return step * times;
    }

    static double linearValueAt(const QPoint &p1, const QPoint &p2, int x) {
        int x1 = p1.x();
        int y1 = p1.y();
        int x2 = p2.x();
        int y2 = p2.y();
        double dx = x2 - x1;
        double dy = y2 - y1;
        double ratio = dy / dx;
        return y1 + (x - x1) * ratio;
    }

    template <typename T>
    static qsizetype binaryFind(QList<T *> &list, T *elem) {
        int low = 0;
        int high = list.count() - 1;
        while (low <= high) {
            int mid = (low + high) / 2;
            if (list[mid]->compareTo(elem) == 0)
                return mid;
            if (list[mid]->compareTo(elem) < 0)
                low = mid + 1;
            else
                high = mid - 1;
        }
        return -1;
    }

    template <typename T>
    static void binaryInsert(QList<T *> &list, T *elem) {
        int low = 0;
        int high = list.count() - 1;
        while (low <= high) {
            int mid = (low + high) / 2;
            if (list[mid]->compareTo(elem) < 0) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        list.insert(low, elem);
    }

    template <typename T>
    static void binaryInsert(QList<T> &list, const T &elem) {
        int low = 0;
        int high = list.count() - 1;
        while (low <= high) {
            int mid = (low + high) / 2;
            if (list[mid].compareTo(&elem) < 0) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        list.insert(low, elem);
    }

    template <typename TItem, typename TContainer>
    static TItem findItemById(const TContainer &container, int id) {
        for (const auto item : container)
            if (item->id() == id)
                return item;
        return nullptr;
    }
};



#endif // MATHUTILS_H
