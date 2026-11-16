#include "TimeAxisLaneView.h"

#include "Utils/FontManager.h"

#include <lite/MusicBase/MusicTime.h>
#include <lite/MusicBase/MusicTimeConverter.h>

#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
    // Seconds-scale divisions in milliseconds. Every entry divides the next one,
    // so zooming out removes lines instead of reflowing them.
    constexpr std::array<double, 16> kStepCandidates = {
        100,    200,     500,     1000,    2000,     5000,     10000,    15000,
        30000,  60000,   120000,  300000,  600000,   900000,   1800000,  3600000};

    // Room a label needs before the next division; matches the marker chips'
    // minimum spacing closely enough that all lanes read the same at a glance.
    constexpr double kMinimumLabelSpacingPx = 64;
    constexpr int kTextLeftPadding = 4;
    constexpr int kMaximumDivisions = 2000;

    // The step lands a division exactly on the minimum spacing and the positions
    // go through ticks, so the spacing comparison needs some slack.
    constexpr double kSpacingTolerancePx = 0.5;
    // Tempo points are ticks, so a segment boundary rarely falls on a division of
    // the millisecond grid. Accept a boundary a hair above a grid line instead of
    // starting the segment one whole step later.
    constexpr double kGridToleranceMs = 1e-6;

    double chooseStep(const double pixelsPerMillisecond) {
        if (pixelsPerMillisecond <= 0)
            return kStepCandidates.front();
        for (const auto step : kStepCandidates) {
            if (step * pixelsPerMillisecond >= kMinimumLabelSpacingPx)
                return step;
        }
        return kStepCandidates.back();
    }

    bool isMinuteBoundary(const double milliseconds) {
        return qRound64(milliseconds) % 60000 == 0;
    }

    bool isWholeSecond(const double milliseconds) {
        return qRound64(milliseconds) % 1000 == 0;
    }
} // namespace

TimeAxisLaneView::TimeAxisLaneView(QWidget *parent) : InfoLaneView(parent) {
    setObjectName("timeAxisLaneView");
}

QList<TimeAxisLaneView::Division> TimeAxisLaneView::divisionsInView() const {
    QList<Division> divisions;
    const auto startTick = visibleStartTick();
    const auto endTick = visibleEndTick();
    if (width() <= 0 || endTick <= startTick)
        return divisions;

    const auto &projectTimeline = timeline();
    const auto pixelsPerTick = width() / (endTick - startTick);
    // Pixel position of the last label; a tempo change can put the first division
    // of a segment inside the gap the previous one already left.
    auto lastLabelX = std::numeric_limits<double>::lowest();

    const auto &tempos = projectTimeline.tempos();
    for (qsizetype index = 0; index < tempos.size(); ++index) {
        const auto &tempo = tempos.at(index);
        if (tempo.pos >= endTick)
            break;
        const auto segmentStartTick = std::max<double>(tempo.pos, startTick);
        const auto segmentEndTick = index + 1 < tempos.size()
                                        ? std::min<double>(tempos.at(index + 1).pos, endTick)
                                        : endTick;
        if (segmentEndTick <= segmentStartTick)
            continue;

        // Milliseconds map linearly onto pixels inside one constant-tempo
        // segment, so each segment picks the step its own density allows. A fast
        // stretch then keeps its labels near the minimum spacing instead of
        // inheriting the coarse step a slow stretch in the same window needs.
        const auto segmentStartMs = projectTimeline.tickToMs(segmentStartTick);
        const auto segmentEndMs = projectTimeline.tickToMs(segmentEndTick);
        const auto pixelsPerMs =
            pixelsPerTick * tempo.value * MusicTime::ticksPerQuarterNote / 60000.0;
        const auto step = chooseStep(pixelsPerMs);
        const auto withTenths = step < 1000;
        const auto atWindowEnd = segmentEndTick >= endTick;

        // The window's left edge cuts through an interval, so the first run keeps
        // the grid line it already passed and lets the painter clip that label at
        // the edge. Every later run starts on the first line it owns.
        auto divisionMs = segmentStartTick <= startTick
                              ? std::floor(segmentStartMs / step) * step
                              : std::ceil((segmentStartMs - kGridToleranceMs) / step) * step;
        if (divisionMs <= 0)
            divisionMs = 0;
        for (; divisionMs < segmentEndMs - kGridToleranceMs ||
               (atWindowEnd && divisionMs <= segmentEndMs + kGridToleranceMs);
             divisionMs += step) {
            if (divisions.size() >= kMaximumDivisions)
                return divisions;
            const auto x = tickToX(projectTimeline.msToTick(divisionMs));
            // Where the new segment starts mid-gap the division keeps its tick
            // line and drops the label: two labels that close would overlap.
            const auto labeled =
                x - lastLabelX + kSpacingTolerancePx >= kMinimumLabelSpacingPx;
            if (labeled)
                lastLabelX = x;
            divisions.append({divisionMs, x, withTenths, labeled});
        }
    }
    return divisions;
}

void TimeAxisLaneView::paintLaneBackground(QPainter *painter) {
    Q_UNUSED(painter);
}

void TimeAxisLaneView::paintLaneContent(QPainter *painter) {
    const auto divisions = divisionsInView();
    if (divisions.isEmpty())
        return;

    // Same ruler geometry as the bar/beat ruler: labels near the top, ticks
    // rising from the bottom edge, shorter as the division gets finer.
    painter->setFont(FontManager::instance().musicUIFont(13));
    const auto majorTickTop = qMax(0, height() - 20);
    const auto minorTickTop = qMax(0, height() - 14);
    const auto faintTickTop = qMax(0, height() - 7);

    for (const auto &division : divisions) {
        if (division.x > width())
            break;

        // Whole minutes read as the "bar" of this axis, whole seconds as
        // "beats", anything finer as subdivisions.
        const auto minute = isMinuteBoundary(division.milliseconds);
        const auto wholeSecond = isWholeSecond(division.milliseconds);
        if (division.x >= 0) {
            painter->setPen(minute   ? barLineColor()
                            : wholeSecond ? beatLineColor()
                                          : commonLineColor());
            painter->drawLine(QLineF(division.x, minute ? majorTickTop
                                                        : wholeSecond ? minorTickTop : faintTickTop,
                                     division.x, height()));
        }
        if (!division.labeled)
            continue;

        // The step keeps neighboring labels apart, so a label is never elided.
        // Near the viewport edge the painter clips whatever hangs over.
        const auto labelStart = division.x + kTextLeftPadding;
        if (labelStart >= width())
            continue;
        painter->setPen(minute ? textColor() : minorTextColor());
        painter->drawText(QRectF(labelStart, 0, width() - labelStart, height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          MusicTimeConverter::formatSecondsLabel(division.milliseconds,
                                                                 division.withTenths));
    }
}

QColor TimeAxisLaneView::minorTextColor() const {
    return m_minorTextColor;
}

void TimeAxisLaneView::setMinorTextColor(const QColor &color) {
    if (m_minorTextColor == color)
        return;
    m_minorTextColor = color;
    update();
}
