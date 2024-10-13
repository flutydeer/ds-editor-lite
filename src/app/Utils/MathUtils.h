//
// Created by fluty on 2024/2/6.
//

#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <QDebug>
#include <QPoint>
#include <cmath>

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

    static double linearValueAt(const QPointF &p1, const QPointF &p2, double x) {
        double x1 = p1.x();
        double y1 = p1.y();
        double x2 = p2.x();
        double y2 = p2.y();
        double dx = x2 - x1;
        double dy = y2 - y1;
        double ratio = dy / dx;
        return y1 + (x - x1) * ratio;
    }

    static double inPowerCurveValueAt(double x, double power) {
        if (x < 0.0 || x > 1.0) {
            qFatal() << "x is not normalized";
            return 0.0;
        }
        return 1 - std::pow(1 - x, power);
    }

    static double inPowerCurveXAt(double y, double power) {
        if (y < 0.0 || y > 1.0) {
            qFatal() << "y is not normalized";
            return 0.0;
        }
        return 1 - std::pow(1 - y, 1 / power);
    }

    static QList<double> resample(const QList<double> &values, double interval,
                                  double newInterval) {
        QList<double> resampledValues;

        int numOldSamples = values.size();
        if (numOldSamples < 2 || interval <= 0 || newInterval <= 0) {
            // 如果数据点不足，或间隔无效，直接返回空的结果
            return resampledValues;
        }

        double totalLength = (numOldSamples - 1) * interval;                 // 总长度
        int numNewSamples = static_cast<int>(totalLength / newInterval) + 1; // 新采样点数目

        for (int i = 0; i < numNewSamples; ++i) {
            double newX = i * newInterval;

            // 找到 newX 所处的区间 [x0, x1]
            int oldIndex = static_cast<int>(newX / interval);
            if (oldIndex >= numOldSamples - 1) {
                break; // 超出范围
            }

            double x0 = oldIndex * interval;
            double x1 = (oldIndex + 1) * interval;
            double y0 = values[oldIndex];
            double y1 = values[oldIndex + 1];

            double newValue = linearValueAt({x0, y0}, {x1, y1}, newX);
            resampledValues.append(newValue);
        }

        return resampledValues;
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
