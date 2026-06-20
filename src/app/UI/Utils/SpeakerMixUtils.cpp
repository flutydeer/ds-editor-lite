#include "SpeakerMixUtils.h"

#include <algorithm>
#include <cmath>

namespace {
    double sumValues(const QVector<double> &values) {
        double total = 0;
        for (const double value : values)
            total += value;
        return total;
    }

    QVector<double> scaleGroup(const QVector<double> &values, const double newTotal) {
        if (values.isEmpty())
            return {};

        const double oldTotal = sumValues(values);

        QVector<double> scaled;
        scaled.reserve(values.size());

        if (qFuzzyIsNull(oldTotal)) {
            const double baseValue = newTotal / values.size();
            for (int i = 0; i < values.size(); ++i)
                scaled.append(baseValue);
        } else {
            const double scale = newTotal / oldTotal;
            for (const double value : values)
                scaled.append(value * scale);
        }

        const double allocated = sumValues(scaled);
        if (!scaled.isEmpty())
            scaled.last() += newTotal - allocated;

        return scaled;
    }
}

namespace SpeakerMixUtils {
    QVector<double> storedWeightsToFull(const QVector<double> &storedWeights, const int fullCount,
                                        const double total) {
        if (fullCount <= 0 || total <= 0)
            return {};

        QVector<double> fullWeights;
        fullWeights.reserve(fullCount);

        double storedTotal = 0;
        for (int i = 0; i < fullCount - 1; ++i) {
            const double value = std::clamp(storedWeights.value(i), 0.0, total);
            fullWeights.append(value);
            storedTotal += value;
        }

        fullWeights.append(std::clamp(total - storedTotal, 0.0, total));
        return normalizeFullWeights(fullWeights, total);
    }

    QVector<double> fullWeightsToStored(const QVector<double> &fullWeights, const double total) {
        const QVector<double> normalized = normalizeFullWeights(fullWeights, total);

        const int storedCount = static_cast<int>(normalized.size()) - 1;

        QVector<double> storedWeights;
        storedWeights.reserve(std::max(0, storedCount));
        for (int i = 0; i < normalized.size() - 1; ++i)
            storedWeights.append(normalized[i]);
        return storedWeights;
    }

    QVector<double> normalizeFullWeights(const QVector<double> &fullWeights, const double total) {
        if (total <= 0)
            return {};

        QVector<double> normalized;
        normalized.reserve(fullWeights.size());

        for (const double value : fullWeights)
            normalized.append(std::clamp(value, 0.0, total));

        if (normalized.isEmpty())
            return normalized;

        const double allocated = sumValues(normalized);
        if (!qFuzzyCompare(allocated, total))
            normalized.last() += total - allocated;

        if (normalized.last() < 0) {
            normalized.last() = 0;
            const double remainingTotal = sumValues(normalized);
            if (!qFuzzyIsNull(remainingTotal)) {
                const double scale = total / remainingTotal;
                for (double &value : normalized)
                    value *= scale;
            } else {
                normalized.first() = total;
            }
        }

        return normalized;
    }

    QVector<int> fullWeightsToPercentages(const QVector<double> &fullWeights, const double total) {
        const QVector<double> normalized = normalizeFullWeights(fullWeights, total);

        QVector<int> values;
        values.reserve(normalized.size());

        QVector<double> fractions;
        fractions.reserve(normalized.size());

        int allocated = 0;
        for (const double weight : normalized) {
            const double percentage = weight * 100.0 / total;
            const int floorValue = static_cast<int>(std::floor(percentage));
            values.append(floorValue);
            fractions.append(percentage - floorValue);
            allocated += floorValue;
        }

        int remaining = 100 - allocated;
        while (remaining > 0 && !values.isEmpty()) {
            int bestIndex = 0;
            for (int i = 1; i < fractions.size(); ++i) {
                if (fractions[i] > fractions[bestIndex])
                    bestIndex = i;
            }
            values[bestIndex]++;
            fractions[bestIndex] = -1.0;
            remaining--;
        }

        return values;
    }

    double cumulativeWeightAtSplit(const QVector<double> &fullWeights, const int splitIndex) {
        double cumulative = 0;
        for (int i = 0; i <= splitIndex && i < fullWeights.size(); ++i)
            cumulative += fullWeights[i];
        return cumulative;
    }

    double snapCumulativeToPercent(const double cumulative, const double total) {
        if (total <= 0)
            return 0;

        const double percent = cumulative * 100.0 / total;
        return std::clamp(std::round(percent) / 100.0 * total, 0.0, total);
    }

    QVector<double> adjacentDragWeights(const QVector<double> &dragStartFullWeights,
                                        const int splitIndex, const double newCumulative,
                                        const double total) {
        QVector<double> values = normalizeFullWeights(dragStartFullWeights, total);
        if (splitIndex < 0 || splitIndex >= values.size() - 1)
            return values;

        const double previousCumulative =
            splitIndex > 0 ? cumulativeWeightAtSplit(values, splitIndex - 1) : 0.0;
        const double nextCumulative = splitIndex < values.size() - 2
                                          ? cumulativeWeightAtSplit(values, splitIndex + 1)
                                          : total;
        const double clampedCumulative =
            std::clamp(newCumulative, previousCumulative, nextCumulative);

        values[splitIndex] = clampedCumulative - previousCumulative;
        values[splitIndex + 1] = nextCumulative - clampedCumulative;
        return normalizeFullWeights(values, total);
    }

    QVector<double> proportionalDragWeights(const QVector<double> &dragStartFullWeights,
                                            const int splitIndex, const double newCumulative,
                                            const double total) {
        const QVector<double> normalized = normalizeFullWeights(dragStartFullWeights, total);
        if (splitIndex < 0 || splitIndex >= normalized.size() - 1)
            return normalized;

        const double clampedCumulative = std::clamp(newCumulative, 0.0, total);

        QVector<double> left;
        QVector<double> right;
        left.reserve(splitIndex + 1);
        right.reserve(normalized.size() - splitIndex - 1);

        for (int i = 0; i < normalized.size(); ++i) {
            if (i <= splitIndex)
                left.append(normalized[i]);
            else
                right.append(normalized[i]);
        }

        QVector<double> values = scaleGroup(left, clampedCumulative);
        values += scaleGroup(right, total - clampedCumulative);
        return normalizeFullWeights(values, total);
    }
}
