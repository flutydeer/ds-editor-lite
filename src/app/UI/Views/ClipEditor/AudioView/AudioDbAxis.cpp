#include "AudioDbAxis.h"

#include <QFontMetricsF>
#include <QPainter>

#include <cmath>

namespace {
    // Base ladder step, widened implicitly by the label spacing rule
    constexpr double kLadderStepDb = 3.0;
    constexpr double kLadderFloorDb = -60.0;
    // Below this the lane is too short to carry any label
    constexpr double kMinHalfLaneHeight = 10.0;
    // Minor ticks that would land closer than this to a labelled one are skipped
    constexpr double kMinorTickMinGap = 3.0;
    constexpr double kMinorTickLength = 2.0;
    constexpr double kMajorTickLength = 5.0;
    constexpr double kLabelGap = 2.0;
    constexpr auto kMinusSign = QChar(0x2212);
    constexpr auto kInfinitySign = QChar(0x221E);

    double amplitudeForDb(const double db) {
        return std::pow(10.0, db / 20.0);
    }
}

QVector<AudioDbAxis::Tick> AudioDbAxis::buildTicks(const double halfLaneHeight,
                                                   const int minLabelSpacing) {
    QVector<Tick> ticks;
    if (halfLaneHeight < kMinHalfLaneHeight || minLabelSpacing <= 0)
        return ticks;

    const auto spacing = static_cast<double>(minLabelSpacing);
    auto lastDistance = halfLaneHeight;
    // Full scale always anchors the top of the axis
    ticks.append({0.0, false, lastDistance});
    for (auto db = -kLadderStepDb; db >= kLadderFloorDb; db -= kLadderStepDb) {
        const auto distance = amplitudeForDb(db) * halfLaneHeight;
        // Everything left is pinned against the centre line: stop here rather
        // than scattering labels over the last few pixels
        if (distance < spacing)
            break;
        if (lastDistance - distance >= spacing) {
            ticks.append({db, false, distance});
            lastDistance = distance;
        }
    }
    ticks.append({0.0, true, 0.0});
    return ticks;
}

QString AudioDbAxis::tickLabel(const Tick &tick) {
    if (tick.infinite)
        return QString(kMinusSign) + kInfinitySign;
    return QString::number(tick.db);
}

void AudioDbAxis::paint(QPainter *painter, const QRectF &laneRect, const QVector<Tick> &ticks,
                        const QString &channelTag, const Colors &colors) {
    if (laneRect.isEmpty() || ticks.isEmpty())
        return;

    const auto centerY = laneRect.center().y();
    const auto halfHeight = laneRect.height() * 0.5;
    const auto right = laneRect.right();
    const auto majorLeft = right - kMajorTickLength;
    const auto minorLeft = right - kMinorTickLength;
    const auto labelRight = majorLeft - kLabelGap;
    const auto metrics = QFontMetricsF(painter->font());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    // Minor ticks: the same 3 dB ladder, drawn where they do not sit on top of a
    // labelled tick
    painter->setPen(QPen(colors.tickColor, 1.0));
    for (auto db = -kLadderStepDb; db >= kLadderFloorDb; db -= kLadderStepDb) {
        const auto distance = amplitudeForDb(db) * halfHeight;
        if (distance < kMinorTickMinGap)
            break;
        auto overlapsLabel = false;
        for (const auto &tick : ticks) {
            if (std::abs(tick.distanceFromCenter - distance) < kMinorTickMinGap) {
                overlapsLabel = true;
                break;
            }
        }
        if (overlapsLabel)
            continue;
        const auto y = qRound(centerY - distance) + 0.5;
        painter->drawLine(QPointF(minorLeft, y), QPointF(right, y));
        const auto mirrorY = qRound(centerY + distance) + 0.5;
        painter->drawLine(QPointF(minorLeft, mirrorY), QPointF(right, mirrorY));
    }

    // Labelled ticks, mirrored into both halves
    for (const auto &tick : ticks) {
        if (tick.infinite)
            continue;
        painter->setPen(QPen(colors.tickColor, 1.0));
        const auto y = qRound(centerY - tick.distanceFromCenter) + 0.5;
        painter->drawLine(QPointF(majorLeft, y), QPointF(right, y));
        const auto mirrorY = qRound(centerY + tick.distanceFromCenter) + 0.5;
        painter->drawLine(QPointF(majorLeft, mirrorY), QPointF(right, mirrorY));

        // Labels only in the top half; the mirror image would double the noise
        // inside a 64 px column without adding information
        const auto textWidth = metrics.horizontalAdvance(tickLabel(tick));
        auto textRect = QRectF(labelRight - textWidth, y - metrics.height() * 0.5, textWidth,
                               metrics.height());
        if (textRect.top() < laneRect.top())
            textRect.moveTop(laneRect.top());
        painter->setPen(colors.labelColor);
        painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, tickLabel(tick));
    }

    // Zero line and its silence label. The label goes just below the line,
    // which is free space because labels are only drawn in the top half.
    painter->setPen(QPen(colors.separatorColor, 1.0));
    const auto zeroY = qRound(centerY) + 0.5;
    painter->drawLine(QPointF(laneRect.left() + 2.0, zeroY), QPointF(right, zeroY));

    const auto infiniteLabel = tickLabel({0.0, true, 0.0});
    const auto infiniteWidth = metrics.horizontalAdvance(infiniteLabel);
    auto infiniteRect = QRectF(labelRight - infiniteWidth, zeroY + 1.0, infiniteWidth,
                               metrics.height());
    if (infiniteRect.bottom() > laneRect.bottom())
        infiniteRect.moveBottom(laneRect.bottom());
    painter->setPen(colors.infiniteLabelColor);
    painter->drawText(infiniteRect, Qt::AlignRight | Qt::AlignVCenter, infiniteLabel);

    // Channel tag in the band's top left corner
    if (!channelTag.isEmpty()) {
        painter->setPen(colors.channelTagColor);
        painter->drawText(QRectF(laneRect.left() + 4.0, laneRect.top() + 1.0,
                                 laneRect.width(), metrics.height()),
                          Qt::AlignLeft | Qt::AlignTop, channelTag);
    }
    painter->restore();
}
