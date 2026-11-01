#ifndef AUDIODBAXIS_H
#define AUDIODBAXIS_H

#include <QColor>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

// Amplitude axis for one audio channel lane.
//
// The waveform is drawn on a linear amplitude scale, so a tick for dB value V
// sits at pow(10, V / 20) of the distance from the lane centre to its edge. The
// pixel gap between equal dB steps therefore shrinks geometrically towards the
// centre, which is why a fixed 3 dB ladder cannot simply be painted: the labels
// have to thin out as the axis approaches silence. Dropping labels that fall
// closer than the minimum spacing does that on its own, and the surviving gaps
// grow from 3 dB to 6 dB, 9 dB and 12 dB, which is the behaviour of a mixing
// console style ruler.
namespace AudioDbAxis {
    struct Tick {
        // dBFS value, meaningless for the silence end
        double db = 0.0;
        // The silence end of the axis, drawn on the lane centre line
        bool infinite = false;
        // Distance from the lane centre towards the lane edge, in pixels
        double distanceFromCenter = 0.0;
    };

    struct Colors {
        QColor tickColor;
        QColor labelColor;
        QColor infiniteLabelColor;
        QColor separatorColor;
        QColor channelTagColor;
    };

    // `halfLaneHeight` is the distance from the lane centre to its edge.
    // The returned ticks are ordered from the edge towards silence.
    [[nodiscard]] QVector<Tick> buildTicks(double halfLaneHeight, int minLabelSpacing);
    [[nodiscard]] QString tickLabel(const Tick &tick);

    // Paints one lane band: minor ticks, labelled ticks, the centre line and the
    // channel tag in the band's top left corner
    void paint(QPainter *painter, const QRectF &laneRect, const QVector<Tick> &ticks,
               const QString &channelTag, const Colors &colors);
}

#endif // AUDIODBAXIS_H
