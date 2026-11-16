#include "TimeAxisLaneView.h"

#include "Utils/FontManager.h"

#include <lite/MusicBase/MusicTimeConverter.h>

#include <QPainter>

#include <array>
#include <cmath>

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
} // namespace

TimeAxisLaneView::TimeAxisLaneView(QWidget *parent) : InfoLaneView(parent) {
    setObjectName("timeAxisLaneView");
}

void TimeAxisLaneView::paintLaneBackground(QPainter *painter) {
    Q_UNUSED(painter);
}

void TimeAxisLaneView::paintLaneContent(QPainter *painter) {
    const auto startTick = visibleStartTick();
    const auto endTick = visibleEndTick();
    if (width() <= 0 || endTick <= startTick)
        return;

    const auto &timeline = this->timeline();
    const auto startMs = timeline.tickToMs(startTick);
    const auto endMs = timeline.tickToMs(endTick);
    const auto spanMs = endMs - startMs;
    if (spanMs <= 0)
        return;

    // Tempo changes make milliseconds-per-pixel vary inside the window, so the
    // step is chosen from the average density and every division is then mapped
    // back to its own tick instead of assuming a uniform pixel grid.
    const auto step = chooseStep(width() / spanMs);
    const auto withTenths = step < 1000;

    // Same ruler geometry as the bar/beat ruler: labels near the top, ticks
    // rising from the bottom edge, shorter as the division gets finer.
    const auto font = FontManager::instance().musicUIFont(13);
    painter->setFont(font);
    const auto majorTickTop = qMax(0, height() - 20);
    const auto minorTickTop = qMax(0, height() - 14);
    const auto faintTickTop = qMax(0, height() - 7);

    auto divisionMs = std::floor(startMs / step) * step;
    if (divisionMs < 0)
        divisionMs = 0;
    const auto nextX = [&](const double ms) {
        return tickToX(timeline.msToTick(ms));
    };

    for (int index = 0; divisionMs <= endMs + 0.5 && index < kMaximumDivisions;
         ++index, divisionMs += step) {
        const auto x = nextX(divisionMs);
        if (x > width())
            break;

        // Whole minutes read as the "bar" of this axis, whole seconds as
        // "beats", anything finer as subdivisions.
        const auto minute = isMinuteBoundary(divisionMs);
        const auto wholeSecond = qRound64(divisionMs) % 1000 == 0;
        if (x >= 0) {
            painter->setPen(minute   ? barLineColor()
                            : wholeSecond ? beatLineColor()
                                          : commonLineColor());
            painter->drawLine(QLineF(x, minute ? majorTickTop
                                              : wholeSecond ? minorTickTop : faintTickTop,
                                     x, height()));
        }

        // The chosen step already keeps neighboring labels apart, so a label is
        // never elided. Near the viewport edge the painter clips whatever hangs
        // over; the label itself is complete at the current zoom level.
        const auto labelStart = x + kTextLeftPadding;
        if (labelStart >= width())
            continue;
        const auto text = MusicTimeConverter::formatSecondsLabel(divisionMs, withTenths);
        painter->setPen(minute ? textColor() : minorTextColor());
        painter->drawText(QRectF(labelStart, 0, width() - labelStart, height()),
                          Qt::AlignLeft | Qt::AlignVCenter, text);
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
