#include "PitchSampling.h"

#include <algorithm>
#include <iterator>

namespace PitchSampling {

    QList<double> resample(const QList<double> &values, const QList<double> &sourcePositions,
                           const QList<double> &targetPositions) {
        if (values.isEmpty() || values.size() != sourcePositions.size())
            return {};
        for (qsizetype i = 1; i < sourcePositions.size(); ++i) {
            if (!(sourcePositions.at(i) > sourcePositions.at(i - 1)))
                return {};
        }

        QList<double> result;
        result.reserve(targetPositions.size());
        for (const double target : targetPositions) {
            if (target <= sourcePositions.first()) {
                result.append(values.first());
                continue;
            }
            if (target >= sourcePositions.last()) {
                result.append(values.last());
                continue;
            }

            const auto right =
                std::lower_bound(sourcePositions.cbegin(), sourcePositions.cend(), target);
            const auto rightIndex = std::distance(sourcePositions.cbegin(), right);
            const auto leftIndex = rightIndex - 1;
            const double x0 = sourcePositions.at(leftIndex);
            const double x1 = sourcePositions.at(rightIndex);
            const double y0 = values.at(leftIndex);
            const double y1 = values.at(rightIndex);
            if (y0 > 0 && y1 > 0) {
                result.append(y0 + (y1 - y0) * (target - x0) / (x1 - x0));
            } else {
                result.append(target - x0 <= x1 - target ? y0 : y1);
            }
        }
        return result;
    }

}
