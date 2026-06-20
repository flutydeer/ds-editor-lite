#ifndef DS_EDITOR_LITE_SPEAKERMIXUTILS_H
#define DS_EDITOR_LITE_SPEAKERMIXUTILS_H

#include <QVector>

namespace SpeakerMixUtils {
    QVector<double> storedWeightsToFull(const QVector<double> &storedWeights, int fullCount,
                                        double total = 1.0);
    QVector<double> fullWeightsToStored(const QVector<double> &fullWeights, double total = 1.0);
    QVector<double> normalizeFullWeights(const QVector<double> &fullWeights, double total = 1.0);
    QVector<int> fullWeightsToPercentages(const QVector<double> &fullWeights, double total = 1.0);

    double cumulativeWeightAtSplit(const QVector<double> &fullWeights, int splitIndex);
    double snapCumulativeToPercent(double cumulative, double total = 1.0);

    QVector<double> adjacentDragWeights(const QVector<double> &dragStartFullWeights, int splitIndex,
                                        double newCumulative, double total = 1.0);
    QVector<double> proportionalDragWeights(const QVector<double> &dragStartFullWeights,
                                            int splitIndex, double newCumulative,
                                            double total = 1.0);
}

#endif // DS_EDITOR_LITE_SPEAKERMIXUTILS_H
