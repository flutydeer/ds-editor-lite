#ifndef TIMEAXISLANEVIEW_H
#define TIMEAXISLANEVIEW_H

#include "InfoLaneView.h"

// Seconds-scale companion of the bar/beat ruler: the row reads the project
// position in seconds ("0:02.6") rather than in ticks, so it stays meaningful
// while the user thinks in the tempo map instead of the musical grid. It is a
// read-only scale, so there are no chips to hit, edit or drag; the row paints
// labels with bottom-anchored ticks in the ruler's visual language plus the
// shared playhead indicators. The step follows the zoom level, and a label that
// hangs over the right edge is clipped by the painter rather than elided.
class TimeAxisLaneView final : public InfoLaneView {
    Q_OBJECT
    Q_PROPERTY(QColor minorTextColor READ minorTextColor WRITE setMinorTextColor)

public:
    explicit TimeAxisLaneView(QWidget *parent = nullptr);

    // Millisecond division in use for the current geometry and visible window.
    // Exposed so the spacing rule can be verified without rendering the row.
    [[nodiscard]] int divisionStepMs() const;

protected:
    // No musical grid on this row: the seconds scale below is the row's own grid.
    void paintLaneBackground(QPainter *painter) override;
    void paintLaneContent(QPainter *painter) override;

private:
    // Slowest tempo inside the visible window. Labels crowd where pixels per
    // millisecond is at its lowest, which is the slow end of the tempo map, so
    // the step has to satisfy the spacing rule there instead of on average.
    [[nodiscard]] double minimumTempoInView() const;

    // Label color of the non-minute divisions; minutes use textColor so the
    // minute marks stand out the way bar numbers do on the ruler.
    [[nodiscard]] QColor minorTextColor() const;
    void setMinorTextColor(const QColor &color);

    QColor m_minorTextColor = {160, 160, 160};
};

#endif // TIMEAXISLANEVIEW_H
