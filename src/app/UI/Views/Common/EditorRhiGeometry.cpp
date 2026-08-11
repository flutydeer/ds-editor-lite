#include "EditorRhiGeometry.h"

#include <QLineF>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {
    EditorRhiSolidVertex vertex(const QPointF &point, const QColor &color, const float coverage) {
        const auto alpha = static_cast<float>(color.alphaF());
        return {static_cast<float>(point.x()),
                static_cast<float>(point.y()),
                static_cast<float>(color.redF()) * alpha,
                static_cast<float>(color.greenF()) * alpha,
                static_cast<float>(color.blueF()) * alpha,
                alpha,
                coverage};
    }

    QPointF normalized(const QPointF &value) {
        const auto length = std::hypot(value.x(), value.y());
        return length > 0.000001 ? value / length : QPointF();
    }

    QPointF normalForSegment(const QPointF &from, const QPointF &to) {
        const auto direction = normalized(to - from);
        return {-direction.y(), direction.x()};
    }

    void appendTriangle(QVector<EditorRhiSolidVertex> &vertices, const QPointF &a, const QPointF &b,
                        const QPointF &c, const QColor &color, const float coverageA,
                        const float coverageB, const float coverageC) {
        vertices.append(vertex(a, color, coverageA));
        vertices.append(vertex(b, color, coverageB));
        vertices.append(vertex(c, color, coverageC));
    }

    QVector<QPointF> roundedRectContour(const QRectF &rect, const double radius) {
        constexpr int segmentsPerCorner = 8;
        constexpr auto pi = std::numbers::pi_v<double>;
        const auto r = std::clamp(radius, 0.0, std::min(rect.width(), rect.height()) * 0.5);
        const std::array centers{
            QPointF(rect.left() + r, rect.top() + r),
            QPointF(rect.right() - r, rect.top() + r),
            QPointF(rect.right() - r, rect.bottom() - r),
            QPointF(rect.left() + r, rect.bottom() - r),
        };
        const std::array startAngles{pi, -pi * 0.5, 0.0, pi * 0.5};

        QVector<QPointF> result;
        result.reserve(centers.size() * (segmentsPerCorner + 1));
        for (qsizetype corner = 0; corner < centers.size(); ++corner) {
            for (int segment = 0; segment <= segmentsPerCorner; ++segment) {
                const auto angle = startAngles[corner] + segment * pi * 0.5 / segmentsPerCorner;
                result.append(centers[corner] + QPointF(std::cos(angle), std::sin(angle)) * r);
            }
        }
        return result;
    }

    void appendContourBand(QVector<EditorRhiSolidVertex> &vertices, const QVector<QPointF> &outer,
                           const QVector<QPointF> &inner, const QColor &color,
                           const float outerCoverage, const float innerCoverage) {
        if (outer.size() != inner.size() || outer.size() < 3)
            return;
        for (qsizetype index = 0; index < outer.size(); ++index) {
            const auto next = (index + 1) % outer.size();
            appendTriangle(vertices, outer[index], outer[next], inner[next], color, outerCoverage,
                           outerCoverage, innerCoverage);
            appendTriangle(vertices, outer[index], inner[next], inner[index], color, outerCoverage,
                           innerCoverage, innerCoverage);
        }
    }

    QVector<QPointF> offsetPolyline(const QVector<QPointF> &points, const double distance,
                                    const double normalSign, const QRectF &clipRect) {
        QVector<QPointF> result;
        result.reserve(points.size());
        for (qsizetype index = 0; index < points.size(); ++index) {
            QPointF normal;
            if (index == 0) {
                normal = normalForSegment(points[0], points[1]) * normalSign;
            } else if (index + 1 == points.size()) {
                normal = normalForSegment(points[index - 1], points[index]) * normalSign;
            } else {
                const auto previous =
                    normalForSegment(points[index - 1], points[index]) * normalSign;
                const auto next = normalForSegment(points[index], points[index + 1]) * normalSign;
                normal = normalized(previous + next);
                const auto denominator = QPointF::dotProduct(normal, next);
                if (normal.isNull() || std::abs(denominator) < 0.0001) {
                    normal = next;
                } else {
                    normal *= std::min(3.0, 1.0 / std::abs(denominator));
                }
            }
            const auto point = points[index] + normal * distance;
            result.append({std::clamp(point.x(), clipRect.left(), clipRect.right()),
                           std::clamp(point.y(), clipRect.top(), clipRect.bottom())});
        }
        return result;
    }

    void appendOpenBand(QVector<EditorRhiSolidVertex> &vertices, const QVector<QPointF> &inner,
                        const QVector<QPointF> &outer, const QColor &color) {
        for (qsizetype index = 0; index + 1 < inner.size(); ++index) {
            appendTriangle(vertices, inner[index], inner[index + 1], outer[index + 1], color, 1.0f,
                           1.0f, 0.0f);
            appendTriangle(vertices, inner[index], outer[index + 1], outer[index], color, 1.0f,
                           0.0f, 0.0f);
        }
    }
}

void EditorRhiGeometry::appendRect(QVector<EditorRhiSolidVertex> &vertices,
                                   const QRectF &physicalRect, const QColor &color,
                                   const float coverage) {
    if (physicalRect.isEmpty() || color.alpha() == 0)
        return;
    const auto topLeft = physicalRect.topLeft();
    const auto topRight = physicalRect.topRight();
    const auto bottomLeft = physicalRect.bottomLeft();
    const auto bottomRight = physicalRect.bottomRight();
    appendTriangle(vertices, topLeft, topRight, bottomRight, color, coverage, coverage, coverage);
    appendTriangle(vertices, topLeft, bottomRight, bottomLeft, color, coverage, coverage, coverage);
}

void EditorRhiGeometry::appendRoundedRect(QVector<EditorRhiSolidVertex> &vertices,
                                          const QRectF &physicalRect, const double radius,
                                          const QColor &color) {
    if (physicalRect.isEmpty() || color.alpha() == 0)
        return;
    const auto r =
        std::clamp(radius, 0.0, std::min(physicalRect.width(), physicalRect.height()) * 0.5);
    if (r <= 0.0) {
        appendRect(vertices, physicalRect, color);
        return;
    }
    constexpr double feather = 0.75;
    const auto innerRect = physicalRect.adjusted(feather, feather, -feather, -feather);
    if (innerRect.isEmpty()) {
        appendRect(vertices, physicalRect, color);
        return;
    }
    const auto inner = roundedRectContour(innerRect, std::max(0.0, r - feather));
    const auto outer = roundedRectContour(
        physicalRect.adjusted(-feather, -feather, feather, feather), r + feather);
    const auto center = innerRect.center();
    for (qsizetype index = 0; index < inner.size(); ++index) {
        const auto next = (index + 1) % inner.size();
        appendTriangle(vertices, center, inner[index], inner[next], color, 1.0f, 1.0f, 1.0f);
    }
    appendContourBand(vertices, outer, inner, color, 0.0f, 1.0f);
}

void EditorRhiGeometry::appendRoundedRectStroke(QVector<EditorRhiSolidVertex> &vertices,
                                                const QRectF &physicalRect, const double radius,
                                                const double width, const QColor &color,
                                                const double feather) {
    if (physicalRect.isEmpty() || width <= 0.0 || color.alpha() == 0)
        return;
    const auto halfWidth = width * 0.5;
    const auto fadeWidth = std::max(0.5, feather);
    const auto outerFade =
        roundedRectContour(physicalRect.adjusted(-halfWidth - fadeWidth, -halfWidth - fadeWidth,
                                                 halfWidth + fadeWidth, halfWidth + fadeWidth),
                           radius + halfWidth + fadeWidth);
    const auto outer = roundedRectContour(
        physicalRect.adjusted(-halfWidth, -halfWidth, halfWidth, halfWidth), radius + halfWidth);
    const auto innerRect = physicalRect.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth);
    if (innerRect.isEmpty()) {
        appendRoundedRect(vertices, physicalRect, radius, color);
        return;
    }
    const auto inner = roundedRectContour(innerRect, std::max(0.0, radius - halfWidth));
    const auto innerFadeRect = innerRect.adjusted(fadeWidth, fadeWidth, -fadeWidth, -fadeWidth);
    if (innerFadeRect.isEmpty()) {
        appendContourBand(vertices, outerFade, outer, color, 0.0f, 1.0f);
        const auto center = innerRect.center();
        for (qsizetype index = 0; index < inner.size(); ++index) {
            const auto next = (index + 1) % inner.size();
            appendTriangle(vertices, center, inner[index], inner[next], color, 1.0f, 1.0f, 1.0f);
        }
        return;
    }
    const auto innerFade =
        roundedRectContour(innerFadeRect, std::max(0.0, radius - halfWidth - fadeWidth));
    appendContourBand(vertices, outerFade, outer, color, 0.0f, 1.0f);
    appendContourBand(vertices, outer, inner, color, 1.0f, 1.0f);
    appendContourBand(vertices, inner, innerFade, color, 1.0f, 0.0f);
}

void EditorRhiGeometry::appendPixelAlignedVerticalLine(QVector<EditorRhiSolidVertex> &vertices,
                                                       const double physicalX, const double top,
                                                       const double bottom, const QColor &color) {
    const auto x = std::round(physicalX);
    appendRect(vertices, QRectF(x, top, 1.0, bottom - top), color);
}

void EditorRhiGeometry::appendPixelAlignedHorizontalLine(QVector<EditorRhiSolidVertex> &vertices,
                                                         const double physicalY, const double left,
                                                         const double right, const QColor &color) {
    const auto y = std::round(physicalY);
    appendRect(vertices, QRectF(left, y, right - left, 1.0), color);
}

void EditorRhiGeometry::appendAntialiasedStroke(QVector<EditorRhiSolidVertex> &vertices,
                                                const QVector<QPointF> &physicalPoints,
                                                const double width, const QColor &color,
                                                const double feather, const double miterLimit) {
    if (physicalPoints.size() < 2 || width < 0.0 || color.alpha() == 0)
        return;

    QVector<QPointF> points;
    points.reserve(physicalPoints.size());
    for (const auto &point : physicalPoints) {
        if (points.isEmpty() || QLineF(points.constLast(), point).length() > 0.001)
            points.append(point);
    }
    if (points.size() < 2)
        return;

    const auto innerHalfWidth = width * 0.5;
    const auto outerHalfWidth = innerHalfWidth + std::max(0.5, feather);
    QVector<QPointF> joins(points.size());
    QVector<double> joinScales(points.size(), 1.0);
    for (qsizetype i = 0; i < points.size(); ++i) {
        if (i == 0) {
            joins[i] = normalForSegment(points[0], points[1]);
        } else if (i + 1 == points.size()) {
            joins[i] = normalForSegment(points[i - 1], points[i]);
        } else {
            const auto previousNormal = normalForSegment(points[i - 1], points[i]);
            const auto nextNormal = normalForSegment(points[i], points[i + 1]);
            auto join = normalized(previousNormal + nextNormal);
            auto denominator = QPointF::dotProduct(join, nextNormal);
            if (join.isNull() || std::abs(denominator) < 0.0001) {
                join = nextNormal;
                denominator = 1.0;
            }
            const auto scale = 1.0 / std::abs(denominator);
            if (scale > miterLimit) {
                joins[i] = nextNormal;
                joinScales[i] = 1.0;
            } else {
                joins[i] = join;
                joinScales[i] = scale;
            }
        }
    }

    struct Section {
        QPointF outerLeft;
        QPointF innerLeft;
        QPointF innerRight;
        QPointF outerRight;
    };

    QVector<Section> sections;
    sections.reserve(points.size());
    for (qsizetype i = 0; i < points.size(); ++i) {
        const auto normal = joins[i] * joinScales[i];
        sections.append({points[i] + normal * outerHalfWidth, points[i] + normal * innerHalfWidth,
                         points[i] - normal * innerHalfWidth, points[i] - normal * outerHalfWidth});
    }

    for (qsizetype i = 0; i + 1 < sections.size(); ++i) {
        const auto &a = sections[i];
        const auto &b = sections[i + 1];
        appendTriangle(vertices, a.outerLeft, b.outerLeft, b.innerLeft, color, 0.0f, 0.0f, 1.0f);
        appendTriangle(vertices, a.outerLeft, b.innerLeft, a.innerLeft, color, 0.0f, 1.0f, 1.0f);
        appendTriangle(vertices, a.innerLeft, b.innerLeft, b.innerRight, color, 1.0f, 1.0f, 1.0f);
        appendTriangle(vertices, a.innerLeft, b.innerRight, a.innerRight, color, 1.0f, 1.0f, 1.0f);
        appendTriangle(vertices, a.innerRight, b.innerRight, b.outerRight, color, 1.0f, 1.0f, 0.0f);
        appendTriangle(vertices, a.innerRight, b.outerRight, a.outerRight, color, 1.0f, 0.0f, 0.0f);
    }

    const auto appendRoundCap = [&](const QPointF &center, const QPointF &direction,
                                    const bool start) {
        constexpr int segmentCount = 10;
        const auto baseAngle = std::atan2(direction.y(), direction.x());
        constexpr auto pi = std::numbers::pi_v<double>;
        const auto firstAngle = baseAngle + (start ? pi * 0.5 : -pi * 0.5);
        const auto angleStep = pi / segmentCount * (start ? 1.0 : -1.0);
        for (int i = 0; i < segmentCount; ++i) {
            const auto angleA = firstAngle + i * angleStep;
            const auto angleB = angleA + angleStep;
            const QPointF innerA =
                center + QPointF(std::cos(angleA), std::sin(angleA)) * innerHalfWidth;
            const QPointF innerB =
                center + QPointF(std::cos(angleB), std::sin(angleB)) * innerHalfWidth;
            const QPointF outerA =
                center + QPointF(std::cos(angleA), std::sin(angleA)) * outerHalfWidth;
            const QPointF outerB =
                center + QPointF(std::cos(angleB), std::sin(angleB)) * outerHalfWidth;
            appendTriangle(vertices, center, innerA, innerB, color, 1.0f, 1.0f, 1.0f);
            appendTriangle(vertices, innerA, outerA, outerB, color, 1.0f, 0.0f, 0.0f);
            appendTriangle(vertices, innerA, outerB, innerB, color, 1.0f, 0.0f, 1.0f);
        }
    };
    appendRoundCap(points.first(), normalized(points.first() - points.at(1)), true);
    appendRoundCap(points.last(), normalized(points.last() - points.at(points.size() - 2)), false);
}

void EditorRhiGeometry::appendAntialiasedHairline(QVector<EditorRhiSolidVertex> &vertices,
                                                  const QVector<QPointF> &physicalPoints,
                                                  const QColor &color, const double feather,
                                                  const double miterLimit) {
    auto hairlineColor = color;
    hairlineColor.setAlphaF(color.alphaF() * 0.75);
    appendAntialiasedStroke(vertices, physicalPoints, 0.0, hairlineColor, feather, miterLimit);
}

void EditorRhiGeometry::appendAntialiasedWaveform(QVector<EditorRhiSolidVertex> &vertices,
                                                  const QVector<QPointF> &physicalTop,
                                                  const QVector<QPointF> &physicalBottom,
                                                  const QRectF &physicalClipRect,
                                                  const QColor &color, const double feather) {
    if (physicalTop.size() != physicalBottom.size() || physicalTop.size() < 2 ||
        physicalClipRect.isEmpty() || color.alpha() == 0) {
        return;
    }

    const auto fadeWidth = std::max(0.5, feather);
    const auto halfFade = fadeWidth * 0.5;
    auto innerTop = offsetPolyline(physicalTop, halfFade, 1.0, physicalClipRect);
    auto innerBottom = offsetPolyline(physicalBottom, halfFade, -1.0, physicalClipRect);
    const auto outerTop = offsetPolyline(physicalTop, halfFade, -1.0, physicalClipRect);
    const auto outerBottom = offsetPolyline(physicalBottom, halfFade, 1.0, physicalClipRect);
    for (qsizetype index = 0; index < innerTop.size(); ++index) {
        if (innerTop[index].y() <= innerBottom[index].y())
            continue;
        const auto centerY = (physicalTop[index].y() + physicalBottom[index].y()) * 0.5;
        innerTop[index].setY(centerY);
        innerBottom[index].setY(centerY);
    }

    for (qsizetype index = 0; index + 1 < innerTop.size(); ++index) {
        appendTriangle(vertices, innerTop[index], innerTop[index + 1], innerBottom[index + 1],
                       color, 1.0f, 1.0f, 1.0f);
        appendTriangle(vertices, innerTop[index], innerBottom[index + 1], innerBottom[index], color,
                       1.0f, 1.0f, 1.0f);
    }
    appendOpenBand(vertices, innerTop, outerTop, color);
    appendOpenBand(vertices, innerBottom, outerBottom, color);

    const auto leftX = std::max(physicalClipRect.left(), physicalTop.first().x() - halfFade);
    const QVector<QPointF> innerLeft{innerTop.first(), innerBottom.first()};
    const QVector<QPointF> outerLeft{
        {leftX, outerTop.first().y()   },
        {leftX, outerBottom.first().y()}
    };
    appendOpenBand(vertices, innerLeft, outerLeft, color);
    const auto rightX = std::min(physicalClipRect.right(), physicalTop.last().x() + halfFade);
    const QVector<QPointF> innerRight{innerBottom.last(), innerTop.last()};
    const QVector<QPointF> outerRight{
        {rightX, outerBottom.last().y()},
        {rightX, outerTop.last().y()   }
    };
    appendOpenBand(vertices, innerRight, outerRight, color);
}
