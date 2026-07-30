#include "EditorRhiGeometry.h"

#include <QLineF>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {
    EditorRhiSolidVertex vertex(const QPointF &point, const QColor &color, const float coverage) {
        const auto alpha = static_cast<float>(color.alphaF());
        return {static_cast<float>(point.x()), static_cast<float>(point.y()),
                static_cast<float>(color.redF()) * alpha,
                static_cast<float>(color.greenF()) * alpha,
                static_cast<float>(color.blueF()) * alpha, alpha, coverage};
    }

    QPointF normalized(const QPointF &value) {
        const auto length = std::hypot(value.x(), value.y());
        return length > 0.000001 ? value / length : QPointF();
    }

    QPointF normalForSegment(const QPointF &from, const QPointF &to) {
        const auto direction = normalized(to - from);
        return {-direction.y(), direction.x()};
    }

    void appendTriangle(QVector<EditorRhiSolidVertex> &vertices, const QPointF &a,
                        const QPointF &b, const QPointF &c, const QColor &color,
                        const float coverageA, const float coverageB, const float coverageC) {
        vertices.append(vertex(a, color, coverageA));
        vertices.append(vertex(b, color, coverageB));
        vertices.append(vertex(c, color, coverageC));
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
    const auto r = std::clamp(radius, 0.0,
                              std::min(physicalRect.width(), physicalRect.height()) * 0.5);
    if (r <= 0.0) {
        appendRect(vertices, physicalRect, color);
        return;
    }
    appendRect(vertices, physicalRect.adjusted(r, 0.0, -r, 0.0), color);
    appendRect(vertices, QRectF(physicalRect.left(), physicalRect.top() + r, r,
                               physicalRect.height() - 2.0 * r), color);
    appendRect(vertices, QRectF(physicalRect.right() - r, physicalRect.top() + r, r,
                               physicalRect.height() - 2.0 * r), color);

    constexpr int segments = 6;
    constexpr auto pi = std::numbers::pi_v<double>;
    const std::array centers{
        QPointF(physicalRect.left() + r, physicalRect.top() + r),
        QPointF(physicalRect.right() - r, physicalRect.top() + r),
        QPointF(physicalRect.right() - r, physicalRect.bottom() - r),
        QPointF(physicalRect.left() + r, physicalRect.bottom() - r),
    };
    const std::array startAngles{pi, -pi * 0.5, 0.0, pi * 0.5};
    for (qsizetype corner = 0; corner < centers.size(); ++corner) {
        for (int i = 0; i < segments; ++i) {
            const auto a = startAngles[corner] + i * pi * 0.5 / segments;
            const auto b = startAngles[corner] + (i + 1) * pi * 0.5 / segments;
            appendTriangle(vertices, centers[corner],
                           centers[corner] + QPointF(std::cos(a), std::sin(a)) * r,
                           centers[corner] + QPointF(std::cos(b), std::sin(b)) * r, color, 1.0f,
                           1.0f, 1.0f);
        }
    }
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
    if (physicalPoints.size() < 2 || width <= 0.0 || color.alpha() == 0)
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
        sections.append({points[i] + normal * outerHalfWidth,
                         points[i] + normal * innerHalfWidth,
                         points[i] - normal * innerHalfWidth,
                         points[i] - normal * outerHalfWidth});
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
            const QPointF innerA = center + QPointF(std::cos(angleA), std::sin(angleA)) *
                                                innerHalfWidth;
            const QPointF innerB = center + QPointF(std::cos(angleB), std::sin(angleB)) *
                                                innerHalfWidth;
            const QPointF outerA = center + QPointF(std::cos(angleA), std::sin(angleA)) *
                                                outerHalfWidth;
            const QPointF outerB = center + QPointF(std::cos(angleB), std::sin(angleB)) *
                                                outerHalfWidth;
            appendTriangle(vertices, center, innerA, innerB, color, 1.0f, 1.0f, 1.0f);
            appendTriangle(vertices, innerA, outerA, outerB, color, 1.0f, 0.0f, 0.0f);
            appendTriangle(vertices, innerA, outerB, innerB, color, 1.0f, 0.0f, 1.0f);
        }
    };
    appendRoundCap(points.first(), normalized(points.first() - points.at(1)), true);
    appendRoundCap(points.last(), normalized(points.last() - points.at(points.size() - 2)), false);
}
