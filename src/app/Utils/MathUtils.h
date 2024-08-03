//
// Created by fluty on 2024/2/6.
//

#ifndef MATHUTILS_H
#define MATHUTILS_H



class MathUtils {
public:
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
};



#endif // MATHUTILS_H
