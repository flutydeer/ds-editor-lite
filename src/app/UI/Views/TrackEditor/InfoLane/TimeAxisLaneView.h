#ifndef TIMEAXISLANEVIEW_H
#define TIMEAXISLANEVIEW_H

#include "InfoLaneView.h"

// Seconds-scale companion of the bar/beat ruler: the row reads the project
// position in seconds ("0:02.6") rather than in ticks, so it stays meaningful
// while the user thinks in the tempo map instead of the musical grid. It is a
// read-only scale, so there are no chips to hit, edit or drag; the row paints
// labels with bottom-anchored ticks in the ruler's visual language plus the
// shared playhead indicators. The division step follows the zoom level together
// with the tempo of the segment it falls in, so a fast stretch of the tempo map
// subdivides the axis more finely than a slow one. A label that hangs over the
// right edge is clipped by the painter rather than elided.
class TimeAxisLaneView final : public InfoLaneView {
    Q_OBJECT
    Q_PROPERTY(QColor minorTextColor READ minorTextColor WRITE setMinorTextColor)

public:
    // One division of the seconds scale as the row paints it for the current
    // geometry, visible window and tempo map.
    struct Division {
        double milliseconds = 0; // position on the project clock
        double x = 0;            // where that position lands on the row
        bool withTenths = true;  // label shape the segment's step calls for
        bool labeled = false;    // false when the label made way for its neighbor
    };

    explicit TimeAxisLaneView(QWidget *parent = nullptr);

    // The divisions to paint, ascending. Exposed so the density rules (the step
    // follows each tempo segment, neighboring labels stay at least the minimum
    // spacing apart) can be verified without rendering the row.
    [[nodiscard]] QList<Division> divisionsInView() const;

protected:
    // No musical grid on this row: the seconds scale below is the row's own grid.
    void paintLaneBackground(QPainter *painter) override;
    void paintLaneContent(QPainter *painter) override;

private:
    // Label color of the non-minute divisions; minutes use textColor so the
    // minute marks stand out the way bar numbers do on the ruler.
    [[nodiscard]] QColor minorTextColor() const;
    void setMinorTextColor(const QColor &color);

    QColor m_minorTextColor = {160, 160, 160};
};

#endif // TIMEAXISLANEVIEW_H
