#include "ScreenSpaceStrokeTessellator.h"

#include <QLineF>

#include <algorithm>
#include <cmath>

namespace {

    constexpr double featherWidth = 1.0;
    constexpr double miterLimit = 4.0;
    constexpr double pi = 3.14159265358979323846;

    struct PhysicalPoint {
        QPointF value;

        [[nodiscard]] QPointF logical(const double scaleX, const double scaleY,
                                      const double devicePixelRatio) const {
            return {value.x() / (scaleX * devicePixelRatio),
                    value.y() / (scaleY * devicePixelRatio)};
        }
    };

    QPointF normalized(const QPointF &value) {
        const auto length = std::hypot(value.x(), value.y());
        return length > 1e-9 ? value / length : QPointF();
    }

    QPointF normalFor(const QPointF &from, const QPointF &to) {
        const auto direction = normalized(to - from);
        return {-direction.y(), direction.x()};
    }

    QColor transparentColor(QColor color) {
        color.setAlpha(0);
        return color;
    }

    void appendTriangle(QVector<ScreenSpaceStrokeVertex> &result, const PhysicalPoint &a,
                        const QColor &aColor, const PhysicalPoint &b, const QColor &bColor,
                        const PhysicalPoint &c, const QColor &cColor, const double scaleX,
                        const double scaleY, const double devicePixelRatio) {
        result.append({
            {a.logical(scaleX, scaleY, devicePixelRatio), aColor},
            {b.logical(scaleX, scaleY, devicePixelRatio), bColor},
            {c.logical(scaleX, scaleY, devicePixelRatio), cColor},
        });
    }

    void appendRoundFan(QVector<ScreenSpaceStrokeVertex> &result, const QPointF &center,
                        const QPointF &fromNormal, const QPointF &toNormal, const double radius,
                        const QColor &color, const double scaleX, const double scaleY,
                        const double devicePixelRatio) {
        auto startAngle = std::atan2(fromNormal.y(), fromNormal.x());
        auto endAngle = std::atan2(toNormal.y(), toNormal.x());
        while (endAngle < startAngle)
            endAngle += 2.0 * pi;
        if (endAngle - startAngle > pi)
            std::swap(startAngle, endAngle);
        const auto sweep = endAngle - startAngle;
        const auto steps = qBound(2, qCeil(std::abs(sweep) * radius / 2.0), 24);
        const auto transparent = transparentColor(color);
        for (int i = 0; i < steps; ++i) {
            const auto a0 = startAngle + sweep * i / steps;
            const auto a1 = startAngle + sweep * (i + 1) / steps;
            const QPointF n0(std::cos(a0), std::sin(a0));
            const QPointF n1(std::cos(a1), std::sin(a1));
            appendTriangle(result, {center}, color, {center + n0 * radius}, color,
                           {center + n1 * radius}, color, scaleX, scaleY, devicePixelRatio);
            appendTriangle(result, {center + n0 * radius}, color,
                           {center + n0 * (radius + featherWidth)}, transparent,
                           {center + n1 * radius}, color, scaleX, scaleY, devicePixelRatio);
            appendTriangle(result, {center + n1 * radius}, color,
                           {center + n0 * (radius + featherWidth)}, transparent,
                           {center + n1 * (radius + featherWidth)}, transparent, scaleX, scaleY,
                           devicePixelRatio);
        }
    }

} // namespace

QVector<ScreenSpaceStrokeVertex> ScreenSpaceStrokeTessellator::tessellate(
    const QVector<QPointF> &points, const QColor &color, const float width, const double scaleX,
    const double scaleY, const double devicePixelRatio, const EditorStrokeJoin join,
    const EditorStrokeCap cap) {
    QVector<ScreenSpaceStrokeVertex> result;
    if (points.size() < 2 || color.alpha() == 0 || scaleX <= 0.0 || scaleY <= 0.0 ||
        devicePixelRatio <= 0.0)
        return result;

    QVector<QPointF> physicalPoints;
    physicalPoints.reserve(points.size());
    for (const auto &point : points) {
        QPointF physical(point.x() * scaleX * devicePixelRatio,
                         point.y() * scaleY * devicePixelRatio);
        if (!physicalPoints.isEmpty() &&
            QLineF(physicalPoints.constLast(), physical).length() < 1e-6)
            continue;
        physicalPoints.append(physical);
    }
    if (physicalPoints.size() < 2)
        return result;

    const auto physicalWidth = std::max(1.0F, width);
    const auto first = physicalPoints.constFirst();
    const auto horizontal = std::ranges::all_of(physicalPoints, [first](const QPointF &point) {
        return qFuzzyCompare(point.y() + 1.0, first.y() + 1.0);
    });
    const auto vertical = std::ranges::all_of(physicalPoints, [first](const QPointF &point) {
        return qFuzzyCompare(point.x() + 1.0, first.x() + 1.0);
    });
    const auto snapOffset = qRound(physicalWidth) % 2 == 0 ? 0.0 : 0.5;
    if (horizontal) {
        const auto snappedY = std::floor(first.y()) + snapOffset;
        for (auto &point : physicalPoints)
            point.setY(snappedY);
    } else if (vertical) {
        const auto snappedX = std::floor(first.x()) + snapOffset;
        for (auto &point : physicalPoints)
            point.setX(snappedX);
    }

    const auto halfWidth = physicalWidth * 0.5;
    const auto transparent = transparentColor(color);
    QVector<QPointF> segmentNormals;
    segmentNormals.reserve(physicalPoints.size() - 1);
    for (qsizetype i = 1; i < physicalPoints.size(); ++i)
        segmentNormals.append(normalFor(physicalPoints.at(i - 1), physicalPoints.at(i)));

    QVector<QPointF> offsets;
    offsets.reserve(physicalPoints.size());
    offsets.append(segmentNormals.first() * halfWidth);
    for (qsizetype i = 1; i + 1 < physicalPoints.size(); ++i) {
        const auto previousNormal = segmentNormals.at(i - 1);
        const auto nextNormal = segmentNormals.at(i);
        const auto miter = normalized(previousNormal + nextNormal);
        const auto denominator = QPointF::dotProduct(miter, nextNormal);
        auto length = std::abs(denominator) > 1e-6 ? halfWidth / denominator : halfWidth;
        if (join != EditorStrokeJoin::Miter || std::abs(length) > halfWidth * miterLimit)
            length = std::copysign(halfWidth, length);
        offsets.append(miter.isNull() ? nextNormal * halfWidth : miter * length);
    }
    offsets.append(segmentNormals.last() * halfWidth);

    for (qsizetype i = 1; i < physicalPoints.size(); ++i) {
        const auto p0 = physicalPoints.at(i - 1);
        const auto p1 = physicalPoints.at(i);
        const auto o0 = offsets.at(i - 1);
        const auto o1 = offsets.at(i);
        const auto n = segmentNormals.at(i - 1);
        const auto outer0 = o0 + normalized(o0) * featherWidth;
        const auto outer1 = o1 + normalized(o1) * featherWidth;

        appendTriangle(result, {p0 + o0}, color, {p0 - o0}, color, {p1 + o1}, color, scaleX, scaleY,
                       devicePixelRatio);
        appendTriangle(result, {p1 + o1}, color, {p0 - o0}, color, {p1 - o1}, color, scaleX, scaleY,
                       devicePixelRatio);
        appendTriangle(result, {p0 + outer0}, transparent, {p0 + o0}, color, {p1 + outer1},
                       transparent, scaleX, scaleY, devicePixelRatio);
        appendTriangle(result, {p1 + outer1}, transparent, {p0 + o0}, color, {p1 + o1}, color,
                       scaleX, scaleY, devicePixelRatio);
        appendTriangle(result, {p0 - o0}, color, {p0 - outer0}, transparent, {p1 - o1}, color,
                       scaleX, scaleY, devicePixelRatio);
        appendTriangle(result, {p1 - o1}, color, {p0 - outer0}, transparent, {p1 - outer1},
                       transparent, scaleX, scaleY, devicePixelRatio);

        if (join == EditorStrokeJoin::Round && i + 1 < physicalPoints.size()) {
            const auto nextNormal = segmentNormals.at(i);
            const auto cross = n.x() * nextNormal.y() - n.y() * nextNormal.x();
            if (cross > 1e-6)
                appendRoundFan(result, p1, n, nextNormal, halfWidth, color, scaleX, scaleY,
                               devicePixelRatio);
            else if (cross < -1e-6)
                appendRoundFan(result, p1, -nextNormal, -n, halfWidth, color, scaleX, scaleY,
                               devicePixelRatio);
        }
    }

    if (cap == EditorStrokeCap::Round) {
        const auto firstNormal = segmentNormals.first();
        const auto lastNormal = segmentNormals.last();
        appendRoundFan(result, physicalPoints.first(), firstNormal, -firstNormal, halfWidth, color,
                       scaleX, scaleY, devicePixelRatio);
        appendRoundFan(result, physicalPoints.last(), -lastNormal, lastNormal, halfWidth, color,
                       scaleX, scaleY, devicePixelRatio);
    } else {
        const auto firstDirection = normalized(physicalPoints.at(1) - physicalPoints.constFirst());
        const auto lastDirection =
            normalized(physicalPoints.constLast() - physicalPoints.at(physicalPoints.size() - 2));
        const auto firstOffset = segmentNormals.constFirst() * halfWidth;
        const auto lastOffset = segmentNormals.constLast() * halfWidth;
        const auto start = physicalPoints.constFirst();
        const auto end = physicalPoints.constLast();
        appendTriangle(result, {start + firstOffset}, color, {start - firstOffset}, color,
                       {start - firstDirection * featherWidth + firstOffset}, transparent, scaleX,
                       scaleY, devicePixelRatio);
        appendTriangle(result, {start - firstOffset}, color,
                       {start - firstDirection * featherWidth - firstOffset}, transparent,
                       {start - firstDirection * featherWidth + firstOffset}, transparent, scaleX,
                       scaleY, devicePixelRatio);
        appendTriangle(result, {end + lastOffset}, color,
                       {end + lastDirection * featherWidth + lastOffset}, transparent,
                       {end - lastOffset}, color, scaleX, scaleY, devicePixelRatio);
        appendTriangle(result, {end - lastOffset}, color,
                       {end + lastDirection * featherWidth + lastOffset}, transparent,
                       {end + lastDirection * featherWidth - lastOffset}, transparent, scaleX,
                       scaleY, devicePixelRatio);
    }
    return result;
}
