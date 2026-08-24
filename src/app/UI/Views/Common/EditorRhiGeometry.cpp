#include "EditorRhiGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {
    struct PremultipliedColor {
        float r;
        float g;
        float b;
        float a;
    };

    PremultipliedColor premultipliedColor(const QColor &color) {
        const auto alpha = static_cast<float>(color.alphaF());
        return {static_cast<float>(color.redF()) * alpha,
                static_cast<float>(color.greenF()) * alpha,
                static_cast<float>(color.blueF()) * alpha, alpha};
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
                        const QPointF &c, const PremultipliedColor &color, const float coverageA,
                        const float coverageB, const float coverageC) {
        vertices.append({static_cast<float>(a.x()), static_cast<float>(a.y()), color.r, color.g,
                         color.b, color.a, coverageA});
        vertices.append({static_cast<float>(b.x()), static_cast<float>(b.y()), color.r, color.g,
                         color.b, color.a, coverageB});
        vertices.append({static_cast<float>(c.x()), static_cast<float>(c.y()), color.r, color.g,
                         color.b, color.a, coverageC});
    }

    Q_ALWAYS_INLINE void writeVertex(EditorRhiSolidVertex *&output, const QPointF &point,
                                     const PremultipliedColor &color, const float coverage) {
        *output++ = {static_cast<float>(point.x()),
                     static_cast<float>(point.y()),
                     color.r,
                     color.g,
                     color.b,
                     color.a,
                     coverage};
    }

    Q_ALWAYS_INLINE void writeTriangle(EditorRhiSolidVertex *&output, const QPointF &a,
                                       const QPointF &b, const QPointF &c,
                                       const PremultipliedColor &color, const float coverageA,
                                       const float coverageB, const float coverageC) {
        writeVertex(output, a, color, coverageA);
        writeVertex(output, b, color, coverageB);
        writeVertex(output, c, color, coverageC);
    }

    constexpr int kCircleSegmentsPerCorner = 8;
    constexpr int kCircleSegmentCount = kCircleSegmentsPerCorner * 4;

    const std::array<QPointF, kCircleSegmentCount> &unitCirclePoints() {
        static const auto points = [] {
            constexpr auto pi = std::numbers::pi_v<double>;
            constexpr std::array startAngles{pi, -pi * 0.5, 0.0, pi * 0.5};
            std::array<QPointF, kCircleSegmentCount> result;
            auto index = 0;
            for (const auto startAngle : startAngles) {
                for (auto segment = 0; segment < kCircleSegmentsPerCorner; ++segment) {
                    const auto angle = startAngle + segment * pi * 0.5 / kCircleSegmentsPerCorner;
                    result[index++] = {std::cos(angle), std::sin(angle)};
                }
            }
            return result;
        }();
        return points;
    }

    QVector<QPointF> roundedRectContour(const QRectF &rect, const double radius,
                                        const bool roundBottomCorners = true) {
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
            if (!roundBottomCorners && corner >= 2) {
                const auto point = corner == 2 ? rect.bottomRight() : rect.bottomLeft();
                for (int segment = 0; segment <= segmentsPerCorner; ++segment)
                    result.append(point);
                continue;
            }
            for (int segment = 0; segment <= segmentsPerCorner; ++segment) {
                const auto angle = startAngles[corner] + segment * pi * 0.5 / segmentsPerCorner;
                result.append(centers[corner] + QPointF(std::cos(angle), std::sin(angle)) * r);
            }
        }
        return result;
    }

    void appendContourBand(QVector<EditorRhiSolidVertex> &vertices, const QVector<QPointF> &outer,
                           const QVector<QPointF> &inner, const PremultipliedColor &color,
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

    struct PixelCoverageSpan {
        double start;
        double end;
        float coverage;
    };

    QVector<PixelCoverageSpan> pixelCoverageSpans(const double start, const double end) {
        QVector<PixelCoverageSpan> result;
        if (end <= start)
            return result;

        const auto firstPixel = std::floor(start);
        const auto lastPixel = std::ceil(end) - 1.0;
        if (firstPixel == lastPixel) {
            result.append({firstPixel, firstPixel + 1.0,
                           static_cast<float>(std::clamp(end - start, 0.0, 1.0))});
            return result;
        }

        result.append({firstPixel, firstPixel + 1.0,
                       static_cast<float>(std::clamp(firstPixel + 1.0 - start, 0.0, 1.0))});
        if (lastPixel > firstPixel + 1.0)
            result.append({firstPixel + 1.0, lastPixel, 1.0f});
        result.append({lastPixel, lastPixel + 1.0,
                       static_cast<float>(std::clamp(end - lastPixel, 0.0, 1.0))});
        return result;
    }

    void appendRoundedRectFill(QVector<EditorRhiSolidVertex> &vertices, const QRectF &physicalRect,
                               const double radius, const QColor &color,
                               const bool roundBottomCorners) {
        if (physicalRect.isEmpty() || color.alpha() == 0)
            return;
        const auto r =
            std::clamp(radius, 0.0, std::min(physicalRect.width(), physicalRect.height()) * 0.5);
        if (r <= 0.0) {
            EditorRhiGeometry::appendRect(vertices, physicalRect, color);
            return;
        }
        constexpr double feather = 0.75;
        const auto innerRect = physicalRect.adjusted(feather, feather, -feather, -feather);
        if (innerRect.isEmpty()) {
            const auto contour = roundedRectContour(physicalRect, r, roundBottomCorners);
            const auto outer =
                roundedRectContour(physicalRect.adjusted(-feather, -feather, feather, feather),
                                   r + feather, roundBottomCorners);
            const auto center = physicalRect.center();
            const auto vertexColor = premultipliedColor(color);
            for (qsizetype index = 0; index < contour.size(); ++index) {
                const auto next = (index + 1) % contour.size();
                appendTriangle(vertices, center, contour[index], contour[next], vertexColor, 1.0f,
                               1.0f, 1.0f);
            }
            appendContourBand(vertices, outer, contour, vertexColor, 0.0f, 1.0f);
            return;
        }
        const auto inner =
            roundedRectContour(innerRect, std::max(0.0, r - feather), roundBottomCorners);
        const auto outer =
            roundedRectContour(physicalRect.adjusted(-feather, -feather, feather, feather),
                               r + feather, roundBottomCorners);
        const auto center = innerRect.center();
        const auto vertexColor = premultipliedColor(color);
        for (qsizetype index = 0; index < inner.size(); ++index) {
            const auto next = (index + 1) % inner.size();
            appendTriangle(vertices, center, inner[index], inner[next], vertexColor, 1.0f, 1.0f,
                           1.0f);
        }
        appendContourBand(vertices, outer, inner, vertexColor, 0.0f, 1.0f);
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
                        const QVector<QPointF> &outer, const PremultipliedColor &color) {
        for (qsizetype index = 0; index + 1 < inner.size(); ++index) {
            appendTriangle(vertices, inner[index], inner[index + 1], outer[index + 1], color, 1.0f,
                           1.0f, 0.0f);
            appendTriangle(vertices, inner[index], outer[index + 1], outer[index], color, 1.0f,
                           0.0f, 0.0f);
        }
    }

    enum class ClipEdge { Left, Right, Top, Bottom };

    bool isInside(const EditorRhiSolidVertex &value, const ClipEdge edge, const QRectF &rect) {
        switch (edge) {
            case ClipEdge::Left:
                return value.x >= rect.left();
            case ClipEdge::Right:
                return value.x <= rect.right();
            case ClipEdge::Top:
                return value.y >= rect.top();
            case ClipEdge::Bottom:
                return value.y <= rect.bottom();
        }
        return false;
    }

    EditorRhiSolidVertex interpolate(const EditorRhiSolidVertex &from,
                                     const EditorRhiSolidVertex &to, const double amount) {
        const auto t = static_cast<float>(std::clamp(amount, 0.0, 1.0));
        const auto mix = [t](const float a, const float b) { return a + (b - a) * t; };
        return {
            mix(from.x, to.x),
            mix(from.y, to.y),
            mix(from.r, to.r),
            mix(from.g, to.g),
            mix(from.b, to.b),
            mix(from.a, to.a),
            mix(from.coverage, to.coverage),
        };
    }

    EditorRhiSolidVertex intersection(const EditorRhiSolidVertex &from,
                                      const EditorRhiSolidVertex &to, const ClipEdge edge,
                                      const QRectF &rect) {
        const auto vertical = edge == ClipEdge::Left || edge == ClipEdge::Right;
        const auto boundary = vertical ? (edge == ClipEdge::Left ? rect.left() : rect.right())
                                       : (edge == ClipEdge::Top ? rect.top() : rect.bottom());
        const auto fromCoordinate = vertical ? from.x : from.y;
        const auto toCoordinate = vertical ? to.x : to.y;
        const auto delta = toCoordinate - fromCoordinate;
        const auto amount = std::abs(delta) > 0.000001 ? (boundary - fromCoordinate) / delta : 0.0;
        auto result = interpolate(from, to, amount);
        if (vertical)
            result.x = static_cast<float>(boundary);
        else
            result.y = static_cast<float>(boundary);
        return result;
    }

    qsizetype clipPolygon(const std::array<EditorRhiSolidVertex, 8> &polygon,
                          const qsizetype polygonSize, const ClipEdge edge, const QRectF &rect,
                          std::array<EditorRhiSolidVertex, 8> &result) {
        if (polygonSize == 0)
            return 0;
        auto resultSize = qsizetype(0);
        auto previous = polygon[polygonSize - 1];
        auto previousInside = isInside(previous, edge, rect);
        for (qsizetype index = 0; index < polygonSize; ++index) {
            const auto &current = polygon[index];
            const auto currentInside = isInside(current, edge, rect);
            if (currentInside != previousInside)
                result[resultSize++] = intersection(previous, current, edge, rect);
            if (currentInside)
                result[resultSize++] = current;
            previous = current;
            previousInside = currentInside;
        }
        return resultSize;
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
    const auto vertexColor = premultipliedColor(color);
    appendTriangle(vertices, topLeft, topRight, bottomRight, vertexColor, coverage, coverage,
                   coverage);
    appendTriangle(vertices, topLeft, bottomRight, bottomLeft, vertexColor, coverage, coverage,
                   coverage);
}

void EditorRhiGeometry::appendClippedTriangles(QVector<EditorRhiSolidVertex> &vertices,
                                               const QVector<EditorRhiSolidVertex> &triangles,
                                               const QRectF &physicalClipRect) {
    const auto clipRect = physicalClipRect.normalized();
    if (clipRect.isEmpty() || triangles.size() < 3)
        return;

    auto minimumX = triangles.constFirst().x;
    auto maximumX = minimumX;
    auto minimumY = triangles.constFirst().y;
    auto maximumY = minimumY;
    for (const auto &value : triangles) {
        minimumX = std::min(minimumX, value.x);
        maximumX = std::max(maximumX, value.x);
        minimumY = std::min(minimumY, value.y);
        maximumY = std::max(maximumY, value.y);
    }
    if (maximumX < clipRect.left() || minimumX > clipRect.right() || maximumY < clipRect.top() ||
        minimumY > clipRect.bottom()) {
        return;
    }
    vertices.reserve(vertices.size() + triangles.size());
    if (triangles.size() % 3 == 0 && minimumX >= clipRect.left() && maximumX <= clipRect.right() &&
        minimumY >= clipRect.top() && maximumY <= clipRect.bottom()) {
        vertices.append(triangles);
        return;
    }

    for (qsizetype index = 0; index + 2 < triangles.size(); index += 3) {
        const auto &a = triangles[index];
        const auto &b = triangles[index + 1];
        const auto &c = triangles[index + 2];
        const auto minimumX = std::min({a.x, b.x, c.x});
        const auto maximumX = std::max({a.x, b.x, c.x});
        const auto minimumY = std::min({a.y, b.y, c.y});
        const auto maximumY = std::max({a.y, b.y, c.y});
        if (maximumX < clipRect.left() || minimumX > clipRect.right() ||
            maximumY < clipRect.top() || minimumY > clipRect.bottom()) {
            continue;
        }
        if (minimumX >= clipRect.left() && maximumX <= clipRect.right() &&
            minimumY >= clipRect.top() && maximumY <= clipRect.bottom()) {
            vertices.append(a);
            vertices.append(b);
            vertices.append(c);
            continue;
        }

        std::array<EditorRhiSolidVertex, 8> buffers[2];
        auto *polygon = &buffers[0];
        auto *clipped = &buffers[1];
        (*polygon)[0] = a;
        (*polygon)[1] = b;
        (*polygon)[2] = c;
        auto polygonSize = qsizetype(3);
        for (const auto edge : {ClipEdge::Left, ClipEdge::Right, ClipEdge::Top, ClipEdge::Bottom}) {
            polygonSize = clipPolygon(*polygon, polygonSize, edge, clipRect, *clipped);
            std::swap(polygon, clipped);
            if (polygonSize < 3)
                break;
        }
        for (qsizetype vertexIndex = 1; vertexIndex + 1 < polygonSize; ++vertexIndex) {
            vertices.append((*polygon)[0]);
            vertices.append((*polygon)[vertexIndex]);
            vertices.append((*polygon)[vertexIndex + 1]);
        }
    }
}

void EditorRhiGeometry::appendRoundedRect(QVector<EditorRhiSolidVertex> &vertices,
                                          const QRectF &physicalRect, const double radius,
                                          const QColor &color) {
    appendRoundedRectFill(vertices, physicalRect, radius, color, true);
}

void EditorRhiGeometry::appendAntialiasedCircle(QVector<EditorRhiSolidVertex> &vertices,
                                                const QPointF &physicalCenter,
                                                const double physicalRadius, const QColor &color) {
    if (physicalRadius <= 0.0 || color.alpha() == 0)
        return;

    constexpr auto feather = 0.75;
    if (physicalRadius <= feather) {
        appendRect(vertices,
                   QRectF(physicalCenter.x() - physicalRadius, physicalCenter.y() - physicalRadius,
                          physicalRadius * 2.0, physicalRadius * 2.0),
                   color);
        return;
    }
    const auto innerRadius = std::max(0.0, physicalRadius - feather);
    const auto outerRadius = physicalRadius + feather;
    const auto &unitPoints = unitCirclePoints();
    const auto vertexColor = premultipliedColor(color);
    const auto vertexOffset = vertices.size();
    vertices.resize(vertexOffset + unitPoints.size() * (innerRadius > 0.0 ? 9 : 3));
    auto *output = vertices.data() + vertexOffset;
    for (qsizetype index = 0; index < unitPoints.size(); ++index) {
        const auto next = (index + 1) % unitPoints.size();
        const auto outerA = physicalCenter + unitPoints[index] * outerRadius;
        const auto outerB = physicalCenter + unitPoints[next] * outerRadius;
        if (innerRadius <= 0.0) {
            writeTriangle(output, physicalCenter, outerA, outerB, vertexColor, 1.0f, 0.0f, 0.0f);
            continue;
        }
        const auto innerA = physicalCenter + unitPoints[index] * innerRadius;
        const auto innerB = physicalCenter + unitPoints[next] * innerRadius;
        writeTriangle(output, physicalCenter, innerA, innerB, vertexColor, 1.0f, 1.0f, 1.0f);
        writeTriangle(output, outerA, outerB, innerB, vertexColor, 0.0f, 0.0f, 1.0f);
        writeTriangle(output, outerA, innerB, innerA, vertexColor, 0.0f, 1.0f, 1.0f);
    }
}

void EditorRhiGeometry::appendTopRoundedRect(QVector<EditorRhiSolidVertex> &vertices,
                                             const QRectF &physicalRect, const double radius,
                                             const QColor &color) {
    appendRoundedRectFill(vertices, physicalRect, radius, color, false);
}

void EditorRhiGeometry::appendRoundedRectStroke(QVector<EditorRhiSolidVertex> &vertices,
                                                const QRectF &physicalRect, const double radius,
                                                const double width, const QColor &color,
                                                const double feather) {
    if (physicalRect.isEmpty() || width <= 0.0 || color.alpha() == 0)
        return;
    const auto vertexColor = premultipliedColor(color);
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
        appendContourBand(vertices, outerFade, outer, vertexColor, 0.0f, 1.0f);
        const auto center = innerRect.center();
        for (qsizetype index = 0; index < inner.size(); ++index) {
            const auto next = (index + 1) % inner.size();
            appendTriangle(vertices, center, inner[index], inner[next], vertexColor, 1.0f, 1.0f,
                           1.0f);
        }
        return;
    }
    const auto innerFade =
        roundedRectContour(innerFadeRect, std::max(0.0, radius - halfWidth - fadeWidth));
    appendContourBand(vertices, outerFade, outer, vertexColor, 0.0f, 1.0f);
    appendContourBand(vertices, outer, inner, vertexColor, 1.0f, 1.0f);
    appendContourBand(vertices, inner, innerFade, vertexColor, 1.0f, 0.0f);
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

void EditorRhiGeometry::appendAntialiasedVerticalLine(QVector<EditorRhiSolidVertex> &vertices,
                                                      const double physicalX, const double top,
                                                      const double bottom,
                                                      const double physicalWidth,
                                                      const QColor &color,
                                                      const double physicalCameraX) {
    if (bottom <= top || physicalWidth <= 0.0 || color.alpha() == 0)
        return;
    const auto viewportCenter = physicalX - physicalCameraX;
    const auto lineStart = viewportCenter - physicalWidth * 0.5;
    const auto lineEnd = viewportCenter + physicalWidth * 0.5;
    for (const auto &span : pixelCoverageSpans(lineStart, lineEnd)) {
        appendRect(vertices,
                   QRectF(span.start + physicalCameraX, top, span.end - span.start, bottom - top),
                   color, span.coverage);
    }
}

void EditorRhiGeometry::appendAntialiasedVerticalOverlay(QVector<EditorRhiOverlayRect> &rects,
                                                         const double physicalViewportX,
                                                         const double top, const double bottom,
                                                         const double physicalWidth,
                                                         const QColor &color) {
    if (bottom <= top || physicalWidth <= 0.0 || color.alpha() == 0)
        return;
    const auto lineStart = physicalViewportX - physicalWidth * 0.5;
    const auto lineEnd = physicalViewportX + physicalWidth * 0.5;
    for (const auto &span : pixelCoverageSpans(lineStart, lineEnd)) {
        rects.append(
            {QRectF(span.start, top, span.end - span.start, bottom - top), color, span.coverage});
    }
}

void EditorRhiGeometry::appendAntialiasedHorizontalLine(QVector<EditorRhiSolidVertex> &vertices,
                                                        const double physicalY, const double left,
                                                        const double right,
                                                        const double physicalWidth,
                                                        const QColor &color,
                                                        const double physicalCameraY) {
    if (right <= left || physicalWidth <= 0.0 || color.alpha() == 0)
        return;
    const auto viewportCenter = physicalY - physicalCameraY;
    const auto lineStart = viewportCenter - physicalWidth * 0.5;
    const auto lineEnd = viewportCenter + physicalWidth * 0.5;
    for (const auto &span : pixelCoverageSpans(lineStart, lineEnd)) {
        appendRect(vertices,
                   QRectF(left, span.start + physicalCameraY, right - left, span.end - span.start),
                   color, span.coverage);
    }
}

void EditorRhiGeometry::appendAntialiasedStroke(QVector<EditorRhiSolidVertex> &vertices,
                                                const QVector<QPointF> &physicalPoints,
                                                const double width, const QColor &color,
                                                const double feather, const double miterLimit,
                                                const Qt::PenCapStyle capStyle,
                                                const Qt::PenJoinStyle joinStyle) {
    if (physicalPoints.size() < 2 || width < 0.0 || color.alpha() == 0)
        return;
    const auto vertexColor = premultipliedColor(color);

    const auto squaredDistance = [](const QPointF &a, const QPointF &b) {
        const auto delta = a - b;
        return QPointF::dotProduct(delta, delta);
    };
    const auto pointsNearlyEqual = [&squaredDistance](const QPointF &a, const QPointF &b) {
        return squaredDistance(a, b) <= 0.000001;
    };
    auto closed = pointsNearlyEqual(physicalPoints.constFirst(), physicalPoints.constLast());
    auto hasNearDuplicate = false;
    for (qsizetype index = 1; index < physicalPoints.size(); ++index) {
        if (pointsNearlyEqual(physicalPoints[index], physicalPoints[index - 1])) {
            hasNearDuplicate = true;
            break;
        }
    }
    QVector<QPointF> filteredPointsStorage;
    const auto needsDistinctPoints = hasNearDuplicate || closed;
    if (needsDistinctPoints) {
        filteredPointsStorage.reserve(physicalPoints.size());
        for (const auto &point : physicalPoints) {
            if (filteredPointsStorage.isEmpty()) {
                filteredPointsStorage.append(point);
                continue;
            }
            if (!pointsNearlyEqual(point, filteredPointsStorage.constLast()))
                filteredPointsStorage.append(point);
        }
        if (closed && filteredPointsStorage.size() > 2 &&
            pointsNearlyEqual(filteredPointsStorage.constFirst(),
                              filteredPointsStorage.constLast()))
            filteredPointsStorage.removeLast();
    }
    auto usesFilteredPoints = needsDistinctPoints;
    const auto &filteredPoints = usesFilteredPoints ? filteredPointsStorage : physicalPoints;
    if (filteredPoints.size() < 2)
        return;
    closed = closed && filteredPoints.size() > 2;

    const auto fadeWidth = std::max(0.5, feather);
    const auto innerHalfWidth = width > 0.0 ? std::max(0.0, width * 0.5 - fadeWidth * 0.5) : 0.0;
    const auto outerHalfWidth = width > 0.0 ? width * 0.5 + fadeWidth * 0.5 : fadeWidth;
    const auto capExtension = capStyle == Qt::SquareCap ? width * 0.5 : 0.0;

    // Avoid overlapping a feathered cap with a subpixel join at the path endpoint.
    const auto endpointJoinDistance = std::max(0.0, fadeWidth * 0.5 - capExtension);
    if (!closed && width > 0.0 && capStyle != Qt::RoundCap && endpointJoinDistance > 0.0 &&
        filteredPoints.size() > 2) {
        const auto endpointJoinDistanceSquared = endpointJoinDistance * endpointJoinDistance;
        if (squaredDistance(filteredPoints[0], filteredPoints[1]) <= endpointJoinDistanceSquared ||
            squaredDistance(filteredPoints[filteredPoints.size() - 2], filteredPoints.last()) <=
                endpointJoinDistanceSquared) {
            if (!usesFilteredPoints) {
                filteredPointsStorage = filteredPoints;
                usesFilteredPoints = true;
            }
            while (filteredPointsStorage.size() > 2 &&
                   squaredDistance(filteredPointsStorage[0], filteredPointsStorage[1]) <=
                       endpointJoinDistanceSquared) {
                filteredPointsStorage.removeAt(1);
            }
            while (filteredPointsStorage.size() > 2 &&
                   squaredDistance(filteredPointsStorage[filteredPointsStorage.size() - 2],
                                   filteredPointsStorage.last()) <= endpointJoinDistanceSquared) {
                filteredPointsStorage.removeAt(filteredPointsStorage.size() - 2);
            }
        }
    }
    const auto &points = usesFilteredPoints ? filteredPointsStorage : physicalPoints;

    if (width == 0.0 && !closed && capStyle == Qt::RoundCap && joinStyle == Qt::MiterJoin) {
        QVector<QPointF> segmentNormals;
        segmentNormals.reserve(points.size() - 1);
        for (qsizetype i = 0; i + 1 < points.size(); ++i) {
            const auto dx = points[i + 1].x() - points[i].x();
            const auto dy = points[i + 1].y() - points[i].y();
            const auto length = std::hypot(dx, dy);
            segmentNormals.append(length > 0.000001 ? QPointF(-dy / length, dx / length)
                                                    : QPointF());
        }
        QVector<QPointF> joins(points.size());
        QVector<double> joinScales(points.size(), 1.0);
        for (qsizetype i = 0; i < points.size(); ++i) {
            if (i == 0) {
                joins[i] = segmentNormals.constFirst();
            } else if (i + 1 == points.size()) {
                joins[i] = segmentNormals.constLast();
            } else {
                const auto &previousNormal = segmentNormals[i - 1];
                const auto &nextNormal = segmentNormals[i];
                auto join = previousNormal + nextNormal;
                const auto joinLength = std::hypot(join.x(), join.y());
                join = joinLength > 0.000001 ? join / joinLength : QPointF();
                auto denominator = join.x() * nextNormal.x() + join.y() * nextNormal.y();
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

        constexpr auto capSegmentCount = 10;

        struct HairlineSection {
            QPointF outerLeft;
            QPointF center;
            QPointF outerRight;
        };

        QVector<HairlineSection> sections;
        sections.reserve(points.size());
        for (qsizetype i = 0; i < points.size(); ++i) {
            const auto offset = joins[i] * (joinScales[i] * outerHalfWidth);
            sections.append({points[i] + offset, points[i], points[i] - offset});
        }

        const auto vertexOffset = vertices.size();
        vertices.resize(vertexOffset + (sections.size() - 1) * 12 + 2 * capSegmentCount * 3);
        auto *output = vertices.data() + vertexOffset;
        for (qsizetype i = 0; i + 1 < sections.size(); ++i) {
            const auto &a = sections[i];
            const auto &b = sections[i + 1];
            writeTriangle(output, a.outerLeft, b.outerLeft, b.center, vertexColor, 0.0f, 0.0f,
                          1.0f);
            writeTriangle(output, a.outerLeft, b.center, a.center, vertexColor, 0.0f, 1.0f, 1.0f);
            writeTriangle(output, a.center, b.center, b.outerRight, vertexColor, 1.0f, 1.0f, 0.0f);
            writeTriangle(output, a.center, b.outerRight, a.outerRight, vertexColor, 1.0f, 0.0f,
                          0.0f);
        }

        const auto appendRoundCap = [&](const QPointF &center, const QPointF &direction,
                                        const bool start) {
            const auto baseAngle = std::atan2(direction.y(), direction.x());
            constexpr auto pi = std::numbers::pi_v<double>;
            const auto firstAngle = baseAngle + (start ? pi * 0.5 : -pi * 0.5);
            const auto angleStep = pi / capSegmentCount * (start ? 1.0 : -1.0);
            for (auto i = 0; i < capSegmentCount; ++i) {
                const auto angleA = firstAngle + i * angleStep;
                const auto angleB = angleA + angleStep;
                const auto outerA =
                    center + QPointF(std::cos(angleA), std::sin(angleA)) * outerHalfWidth;
                const auto outerB =
                    center + QPointF(std::cos(angleB), std::sin(angleB)) * outerHalfWidth;
                writeTriangle(output, center, outerA, outerB, vertexColor, 1.0f, 0.0f, 0.0f);
            }
        };
        appendRoundCap(points.first(), normalized(points.first() - points.at(1)), true);
        appendRoundCap(points.last(), normalized(points.last() - points.at(points.size() - 2)),
                       false);
        return;
    }

    struct Section {
        QPointF outerLeft;
        QPointF innerLeft;
        QPointF innerRight;
        QPointF outerRight;
    };

    QVector<Section> sections;
    sections.reserve(points.size() * 2);
    const auto appendEndpoint = [&](const QPointF &point, const QPointF &direction,
                                    const QPointF &normal, const bool start) {
        const auto sign = start ? -1.0 : 1.0;
        auto innerCenter = point + direction * sign * capExtension;
        auto outerCenter = innerCenter;
        if (capStyle != Qt::RoundCap && width > 0.0) {
            innerCenter -= direction * sign * fadeWidth * 0.5;
            outerCenter += direction * sign * fadeWidth * 0.5;
        }
        sections.append(
            {outerCenter + normal * outerHalfWidth, innerCenter + normal * innerHalfWidth,
             innerCenter - normal * innerHalfWidth, outerCenter - normal * outerHalfWidth});
    };
    const auto appendJoin = [&](const QPointF &point, const QPointF &previousDirection,
                                const QPointF &nextDirection) {
        const QPointF previousNormal(-previousDirection.y(), previousDirection.x());
        const QPointF nextNormal(-nextDirection.y(), nextDirection.x());
        const auto turn = std::atan2(previousDirection.x() * nextDirection.y() -
                                         previousDirection.y() * nextDirection.x(),
                                     QPointF::dotProduct(previousDirection, nextDirection));
        auto miterNormal = normalized(previousNormal + nextNormal);
        auto denominator = QPointF::dotProduct(miterNormal, nextNormal);
        if (miterNormal.isNull() || std::abs(denominator) < 0.0001) {
            miterNormal = nextNormal;
            denominator = 1.0;
        }
        const auto miterScale = 1.0 / std::abs(denominator);
        const auto appendMiter = [&] {
            const auto outerNormal = miterNormal * (outerHalfWidth * miterScale);
            const auto innerNormal = miterNormal * (innerHalfWidth * miterScale);
            sections.append({point + outerNormal, point + innerNormal, point - innerNormal,
                             point - outerNormal});
        };
        if (std::abs(turn) < 0.0001 || (joinStyle == Qt::MiterJoin && miterScale <= miterLimit)) {
            appendMiter();
            return;
        }

        const auto boundedMiterScale = std::min(miterScale, std::max(1.0, miterLimit));
        const auto outerMiter = miterNormal * (outerHalfWidth * boundedMiterScale);
        const auto innerMiter = miterNormal * (innerHalfWidth * boundedMiterScale);
        const auto roundJoin = joinStyle == Qt::RoundJoin;
        const auto segmentCount =
            roundJoin ? std::max(1, static_cast<int>(std::ceil(std::abs(turn) /
                                                               (std::numbers::pi_v<double> / 8.0))))
                      : 1;
        const auto outerOnRight = turn > 0.0;
        const auto firstOuterDirection = outerOnRight ? -previousNormal : previousNormal;
        for (int segment = 0; segment <= segmentCount; ++segment) {
            const auto angle = turn * segment / segmentCount;
            const auto cosine = std::cos(angle);
            const auto sine = std::sin(angle);
            const QPointF outerDirection(
                firstOuterDirection.x() * cosine - firstOuterDirection.y() * sine,
                firstOuterDirection.x() * sine + firstOuterDirection.y() * cosine);
            if (outerOnRight) {
                sections.append({point + outerMiter, point + innerMiter,
                                 point + outerDirection * innerHalfWidth,
                                 point + outerDirection * outerHalfWidth});
            } else {
                sections.append({point + outerDirection * outerHalfWidth,
                                 point + outerDirection * innerHalfWidth, point - innerMiter,
                                 point - outerMiter});
            }
        }
    };

    if (closed) {
        for (qsizetype i = 0; i < points.size(); ++i) {
            const auto previous = (i + points.size() - 1) % points.size();
            const auto next = (i + 1) % points.size();
            appendJoin(points[i], normalized(points[i] - points[previous]),
                       normalized(points[next] - points[i]));
        }
    } else {
        const auto firstDirection = normalized(points[1] - points[0]);
        appendEndpoint(points[0], firstDirection, QPointF(-firstDirection.y(), firstDirection.x()),
                       true);
        for (qsizetype i = 1; i + 1 < points.size(); ++i) {
            appendJoin(points[i], normalized(points[i] - points[i - 1]),
                       normalized(points[i + 1] - points[i]));
        }
        const auto lastDirection = normalized(points.last() - points[points.size() - 2]);
        appendEndpoint(points.last(), lastDirection, QPointF(-lastDirection.y(), lastDirection.x()),
                       false);
    }

    const auto sectionCount = closed ? sections.size() : sections.size() - 1;
    constexpr auto roundCapSegmentCount = 12;
    const auto capVertexCount = closed                     ? 0
                                : capStyle == Qt::RoundCap ? 2 * roundCapSegmentCount * 9
                                                           : 12;
    vertices.reserve(vertices.size() + sectionCount * 18 + capVertexCount);
    for (qsizetype i = 0; i < sectionCount; ++i) {
        const auto &a = sections[i];
        const auto &b = sections[(i + 1) % sections.size()];
        appendTriangle(vertices, a.outerLeft, b.outerLeft, b.innerLeft, vertexColor, 0.0f, 0.0f,
                       1.0f);
        appendTriangle(vertices, a.outerLeft, b.innerLeft, a.innerLeft, vertexColor, 0.0f, 1.0f,
                       1.0f);
        appendTriangle(vertices, a.innerLeft, b.innerLeft, b.innerRight, vertexColor, 1.0f, 1.0f,
                       1.0f);
        appendTriangle(vertices, a.innerLeft, b.innerRight, a.innerRight, vertexColor, 1.0f, 1.0f,
                       1.0f);
        appendTriangle(vertices, a.innerRight, b.innerRight, b.outerRight, vertexColor, 1.0f, 1.0f,
                       0.0f);
        appendTriangle(vertices, a.innerRight, b.outerRight, a.outerRight, vertexColor, 1.0f, 0.0f,
                       0.0f);
    }

    if (closed)
        return;

    const auto appendFlatCap = [&](const Section &section) {
        appendTriangle(vertices, section.outerLeft, section.innerLeft, section.innerRight,
                       vertexColor, 0.0f, 1.0f, 1.0f);
        appendTriangle(vertices, section.outerLeft, section.innerRight, section.outerRight,
                       vertexColor, 0.0f, 1.0f, 0.0f);
    };
    if (capStyle != Qt::RoundCap) {
        appendFlatCap(sections.first());
        appendFlatCap(sections.last());
        return;
    }

    const auto appendRoundCap = [&](const QPointF &center, const QPointF &direction) {
        const auto baseAngle = std::atan2(direction.y(), direction.x());
        constexpr auto pi = std::numbers::pi_v<double>;
        const auto firstAngle = baseAngle - pi * 0.5;
        const auto angleStep = pi / roundCapSegmentCount;
        for (int i = 0; i < roundCapSegmentCount; ++i) {
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
            appendTriangle(vertices, center, innerA, innerB, vertexColor, 1.0f, 1.0f, 1.0f);
            appendTriangle(vertices, innerA, outerA, outerB, vertexColor, 1.0f, 0.0f, 0.0f);
            appendTriangle(vertices, innerA, outerB, innerB, vertexColor, 1.0f, 0.0f, 1.0f);
        }
    };
    appendRoundCap(points.first(), normalized(points.first() - points.at(1)));
    appendRoundCap(points.last(), normalized(points.last() - points.at(points.size() - 2)));
}

void EditorRhiGeometry::appendAntialiasedDashedStroke(
    QVector<EditorRhiSolidVertex> &vertices, const QVector<QPointF> &physicalPoints,
    const double width, const QColor &color, const double dashLength, const double gapLength,
    const double feather, const double miterLimit, const Qt::PenCapStyle capStyle,
    const Qt::PenJoinStyle joinStyle) {
    if (physicalPoints.size() < 2 || dashLength <= 0.0 || gapLength <= 0.0)
        return;

    auto drawing = true;
    auto remaining = dashLength;
    QVector<QPointF> dash{physicalPoints.first()};
    for (int index = 0; index + 1 < physicalPoints.size(); ++index) {
        auto cursor = physicalPoints.at(index);
        const auto end = physicalPoints.at(index + 1);
        auto segment = end - cursor;
        auto segmentLength = std::hypot(segment.x(), segment.y());
        while (segmentLength > 0.001) {
            const auto advance = std::min(remaining, segmentLength);
            const auto next = cursor + segment * (advance / segmentLength);
            if (drawing) {
                if (dash.isEmpty())
                    dash.append(cursor);
                dash.append(next);
            }
            cursor = next;
            segment = end - cursor;
            segmentLength = std::hypot(segment.x(), segment.y());
            remaining -= advance;
            if (remaining <= 0.001) {
                if (drawing && dash.size() >= 2) {
                    appendAntialiasedStroke(vertices, dash, width, color, feather, miterLimit,
                                            capStyle, joinStyle);
                }
                dash.clear();
                drawing = !drawing;
                remaining = drawing ? dashLength : gapLength;
            }
        }
    }
    if (drawing && dash.size() >= 2) {
        appendAntialiasedStroke(vertices, dash, width, color, feather, miterLimit, capStyle,
                                joinStyle);
    }
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
    const auto vertexColor = premultipliedColor(color);

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
                       vertexColor, 1.0f, 1.0f, 1.0f);
        appendTriangle(vertices, innerTop[index], innerBottom[index + 1], innerBottom[index],
                       vertexColor, 1.0f, 1.0f, 1.0f);
    }
    appendOpenBand(vertices, innerTop, outerTop, vertexColor);
    appendOpenBand(vertices, innerBottom, outerBottom, vertexColor);

    const auto leftX = std::max(physicalClipRect.left(), physicalTop.first().x() - halfFade);
    const QVector<QPointF> innerLeft{innerTop.first(), innerBottom.first()};
    const QVector<QPointF> outerLeft{
        {leftX, outerTop.first().y()   },
        {leftX, outerBottom.first().y()}
    };
    appendOpenBand(vertices, innerLeft, outerLeft, vertexColor);
    const auto rightX = std::min(physicalClipRect.right(), physicalTop.last().x() + halfFade);
    const QVector<QPointF> innerRight{innerBottom.last(), innerTop.last()};
    const QVector<QPointF> outerRight{
        {rightX, outerBottom.last().y()},
        {rightX, outerTop.last().y()   }
    };
    appendOpenBand(vertices, innerRight, outerRight, vertexColor);
}
