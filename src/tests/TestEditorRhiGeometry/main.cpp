#include <QtTest/QTest>
#include "UI/Views/Common/EditorRhiGeometry.h"
#include "UI/Views/Common/EditorItemGeometry.h"

#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace {




    bool equalVertex(const EditorRhiSolidVertex &lhs, const EditorRhiSolidVertex &rhs) {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.r == rhs.r && lhs.g == rhs.g &&
               lhs.b == rhs.b && lhs.a == rhs.a && lhs.coverage == rhs.coverage;
    }

    double twiceTriangleArea(const EditorRhiSolidVertex &a, const EditorRhiSolidVertex &b,
                             const EditorRhiSolidVertex &c) {
        return std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
    }
}

class EditorRhiGeometryTests final : public QObject {
    Q_OBJECT

private slots:

    void minimumItemGeometry() {
        const QRectF clipRect(0.0, 0.0, 10.0, 10.0);
        const QRectF ordinaryNoteModelRect(2.0, 3.0, 10.0, 8.0);
        const auto ordinaryNoteRect = EditorItemGeometry::notePaintRect(ordinaryNoteModelRect);
        QVERIFY2((ordinaryNoteRect == QRectF(3.5, 4.5, 7.0, 5.0)),
                 "rounded item padding must preserve ordinary geometry");

        const QRectF minimumNoteModelRect(4.0, 3.0, 0.25, 8.0);
        const auto minimumNoteRect = EditorItemGeometry::notePaintRect(minimumNoteModelRect);
        QVERIFY2((minimumNoteRect == QRectF(3.625, 4.5, 1.0, 5.0) &&
                  minimumNoteRect.center().x() == minimumNoteModelRect.center().x()),
                 "a subpixel note must use a centered one-pixel visual body");
        const QRectF minimumClipModelRect(4.0, 3.0, 0.25, 8.0);
        const auto minimumClipRect = EditorItemGeometry::clipPaintRect(minimumClipModelRect);
        const QRectF expectedMinimumClipRect(3.625, 3.0 + EditorItemGeometry::clipVerticalPadding,
                                             1.0,
                                             8.0 - EditorItemGeometry::clipVerticalPadding * 2.0);
        QVERIFY2((minimumClipRect == expectedMinimumClipRect &&
                  minimumClipRect.center().x() == minimumClipModelRect.center().x()),
                 "a subpixel clip must use a centered one-pixel visual body");

        const QRectF zoomedNoteModelRect(4.0, 3.0, 20.0, 8.0);
        const auto zoomedNoteRect = EditorItemGeometry::notePaintRect(zoomedNoteModelRect);
        QVERIFY2(
            (zoomedNoteRect.width() ==
             zoomedNoteModelRect.width() - EditorItemGeometry::noteBorderWidth * 2.0),
            "a zoomed one-tick note must use its scaled model width instead of the visual minimum");
        const QRectF zoomedClipModelRect(4.0, 3.0, 20.0, 8.0);
        const auto zoomedClipRect = EditorItemGeometry::clipPaintRect(zoomedClipModelRect);
        QVERIFY2(
            (zoomedClipRect.width() ==
             zoomedClipModelRect.width() - EditorItemGeometry::clipHorizontalPadding * 2.0),
            "a zoomed one-tick clip must use its scaled model width instead of the visual minimum");
        const auto highDpiNoteRect =
            EditorItemGeometry::notePaintRect(QRectF(8.0, 6.0, 0.5, 16.0), 2.0);
        QVERIFY2((highDpiNoteRect ==
                  QRectF(minimumNoteRect.topLeft() * 2.0, minimumNoteRect.size() * 2.0)),
                 "minimum visual geometry must scale with the device pixel ratio");

        QVERIFY2((EditorItemGeometry::notePaintRect(QRectF(4.0, 3.0, 0.0, 8.0)).isEmpty()),
                 "visual styling must not revive zero-width model geometry");
        QVERIFY2(
            (EditorItemGeometry::adaptiveCornerRadius(
                 minimumNoteRect, EditorItemGeometry::noteCornerRadius) == 0.5 &&
             EditorItemGeometry::adaptiveCornerRadius(
                 minimumClipRect, EditorItemGeometry::clipCornerRadius) == 0.5 &&
             EditorItemGeometry::adaptiveCornerRadius(QRectF(0.0, 0.0, 4.0, 20.0), 6.0) == 2.0),
            "rounded item corners must adapt to the available width and height");

        QVector<EditorRhiSolidVertex> minimumRoundedClip;
        EditorRhiGeometry::appendRoundedRect(
            minimumRoundedClip, minimumClipRect,
            EditorItemGeometry::adaptiveCornerRadius(minimumClipRect,
                                                     EditorItemGeometry::clipCornerRadius),
            QColor(255, 255, 255));
        QVERIFY2((minimumRoundedClip.size() > 6),
                 "a narrow rounded item must not fall back to a square rectangle");
        QVERIFY2(
            (std::any_of(minimumRoundedClip.cbegin(), minimumRoundedClip.cend(),
                         [](const EditorRhiSolidVertex &value) { return value.coverage == 0.0f; })),
            "a narrow rounded item must retain antialiased corner coverage");

        QImage legacyClipImage(32, 80, QImage::Format_ARGB32_Premultiplied);
        legacyClipImage.fill(Qt::transparent);
        const auto legacyClipRect =
            EditorItemGeometry::clipPaintRect(QRectF(14.0, 4.0, 0.25, 72.0));
        const auto legacyClipRadius = EditorItemGeometry::adaptiveCornerRadius(
            legacyClipRect, EditorItemGeometry::clipCornerRadius);
        {
            QPainter painter(&legacyClipImage);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(Qt::white, EditorItemGeometry::clipBorderWidth));
            painter.setBrush(Qt::white);
            painter.drawRoundedRect(legacyClipRect, legacyClipRadius, legacyClipRadius);
        }
        QRect legacyClipPixels;
        for (auto y = 0; y < legacyClipImage.height(); ++y) {
            for (auto x = 0; x < legacyClipImage.width(); ++x) {
                if (qAlpha(legacyClipImage.pixel(x, y)) != 0)
                    legacyClipPixels |= QRect(x, y, 1, 1);
            }
        }
        QVERIFY2((legacyClipPixels.width() >= 2 && legacyClipPixels.width() <= 4),
                 "the Legacy narrow clip must remain visible without overlapping corner flares");

        QVector<EditorRhiSolidVertex> minimumRoundedNote;
        EditorRhiGeometry::appendRoundedRect(
            minimumRoundedNote, minimumNoteRect,
            EditorItemGeometry::adaptiveCornerRadius(minimumNoteRect,
                                                     EditorItemGeometry::noteCornerRadius),
            Qt::white);
        QVERIFY2(
            (minimumRoundedNote.size() > 6 &&
             std::any_of(minimumRoundedNote.cbegin(), minimumRoundedNote.cend(),
                         [](const EditorRhiSolidVertex &value) { return value.coverage == 0.0f; })),
            "a minimum-width RHI note must retain antialiased rounded corners");
    }

    void triangleClipping() {
        const QRectF clipRect(0.0, 0.0, 10.0, 10.0);
        const QVector<EditorRhiSolidVertex> inside{
            {1.0f, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
            {8.0f, 1.0f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f},
            {1.0f, 8.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f},
        };
        QVector<EditorRhiSolidVertex> accepted;
        EditorRhiGeometry::appendClippedTriangles(accepted, inside, clipRect);
        QVERIFY2((accepted.size() == inside.size()),
                 "fully visible triangles must be appended intact");
        if (accepted.size() == inside.size()) {
            for (qsizetype index = 0; index < inside.size(); ++index)
                QVERIFY2((equalVertex(accepted[index], inside[index])),
                         "the clipping fast path must preserve every vertex attribute");
        }

        const QVector<EditorRhiSolidVertex> outside{
            {-8.0f, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
            {-2.0f, 1.0f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f},
            {-3.0f, 8.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f},
        };
        QVector<EditorRhiSolidVertex> rejected;
        EditorRhiGeometry::appendClippedTriangles(rejected, outside, clipRect);
        QVERIFY2((rejected.isEmpty()), "fully hidden triangles must be rejected as a batch");

        const QVector<EditorRhiSolidVertex> intersecting{
            {-2.0f, 2.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
            {5.0f,  2.0f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f},
            {2.0f,  8.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f},
        };
        QVector<EditorRhiSolidVertex> clipped;
        EditorRhiGeometry::appendClippedTriangles(clipped, intersecting, clipRect);
        QVERIFY2((clipped.size() == 6),
                 "a triangle crossing one clip edge must become two triangles");
        for (const auto &value : clipped) {
            QVERIFY2((clipRect.contains(QPointF(value.x, value.y))),
                     "partially clipped vertices must stay inside the clip rectangle");
            QVERIFY2((value.coverage >= 0.5f && value.coverage <= 0.7f),
                     "clipping must interpolate coverage without leaving the source range");
        }
    }

    void roundedStrokeClipping() {
        const QRectF clipRect(0.0, 0.0, 10.0, 10.0);
        QVector<EditorRhiSolidVertex> roundedStroke;
        EditorRhiGeometry::appendRoundedRectStroke(roundedStroke, QRectF(2.0, -2.0, 6.0, 8.0), 2.0,
                                                   1.2, QColor(255, 255, 255));
        QVector<EditorRhiSolidVertex> clippedRoundedStroke;
        EditorRhiGeometry::appendClippedTriangles(clippedRoundedStroke, roundedStroke, clipRect);
        QVERIFY2((!clippedRoundedStroke.isEmpty()),
                 "a partially visible rounded stroke must retain visible geometry");
        for (const auto &value : clippedRoundedStroke) {
            QVERIFY2((clipRect.contains(QPointF(value.x, value.y))),
                     "clipped rounded-stroke vertices must stay inside the clip rectangle");
        }
        QVERIFY2(
            (std::any_of(clippedRoundedStroke.cbegin(), clippedRoundedStroke.cend(),
                         [](const EditorRhiSolidVertex &value) { return value.coverage == 0.0f; })),
            "clipping must preserve rounded-stroke antialiasing coverage");
    }

    void hairlineDeduplication() {
        const QRectF clipRect(0.0, 0.0, 10.0, 10.0);
        QVector<EditorRhiSolidVertex> hairline;
        EditorRhiGeometry::appendAntialiasedHairline(hairline,
                                                     {
                                                         {2.0, 5.0},
                                                         {8.0, 5.0}
        },
                                                     QColor(255, 255, 255));
        QVERIFY2((!hairline.isEmpty() && hairline.size() % 3 == 0),
                 "a single hairline segment must omit zero-area stroke and cap triangles");
        for (qsizetype index = 0; index + 2 < hairline.size(); index += 3) {
            QVERIFY2((twiceTriangleArea(hairline[index], hairline[index + 1], hairline[index + 2]) >
                      1e-6),
                     "hairline geometry must not contain degenerate triangles");
        }

        QVector<EditorRhiSolidVertex> joinedHairline;
        EditorRhiGeometry::appendAntialiasedHairline(joinedHairline,
                                                     {
                                                         {2.0, 5.0},
                                                         {5.0, 3.0},
                                                         {8.0, 5.0}
        },
                                                     QColor(255, 255, 255));
        QVERIFY2((!joinedHairline.isEmpty() && joinedHairline.size() % 3 == 0),
                 "a joined hairline must produce complete triangles");
        for (qsizetype index = 0; index + 2 < joinedHairline.size(); index += 3) {
            QVERIFY2((twiceTriangleArea(joinedHairline[index], joinedHairline[index + 1],
                                        joinedHairline[index + 2]) > 1e-6),
                     "joined hairline geometry must not contain degenerate triangles");
        }

        QVector<EditorRhiSolidVertex> deduplicatedHairline;
        EditorRhiGeometry::appendAntialiasedHairline(deduplicatedHairline,
                                                     {
                                                         {2.0, 5.0},
                                                         {2.0, 5.0},
                                                         {8.0, 5.0}
        },
                                                     QColor(255, 255, 255));
        QVERIFY2((deduplicatedHairline.size() == hairline.size()),
                 "duplicate hairline points must be removed before tessellation");
        if (deduplicatedHairline.size() == hairline.size()) {
            for (qsizetype index = 0; index < hairline.size(); ++index) {
                QVERIFY2((equalVertex(deduplicatedHairline[index], hairline[index])),
                         "duplicate removal must preserve the generated hairline vertices");
            }
        }
    }

    void shortStrokeEndCaps() {
        const QRectF clipRect(0.0, 0.0, 10.0, 10.0);
        const QVector<QPointF> shortEndpointSegments{
            {2.0,  5.0},
            {2.25, 5.0},
            {7.75, 5.0},
            {8.0,  5.0}
        };
        const QVector<QPointF> simplifiedEndpointSegments{
            {2.0, 5.0},
            {8.0, 5.0}
        };
        QVector<EditorRhiSolidVertex> adjustedFlatCap;
        EditorRhiGeometry::appendAntialiasedStroke(adjustedFlatCap, shortEndpointSegments, 1.5,
                                                   QColor(255, 255, 255), 1.0, 3.0, Qt::FlatCap,
                                                   Qt::RoundJoin);
        QVector<EditorRhiSolidVertex> simplifiedFlatCap;
        EditorRhiGeometry::appendAntialiasedStroke(simplifiedFlatCap, simplifiedEndpointSegments,
                                                   1.5, QColor(255, 255, 255), 1.0, 3.0,
                                                   Qt::FlatCap, Qt::RoundJoin);
        QVERIFY2((adjustedFlatCap.size() == simplifiedFlatCap.size()),
                 "flat caps must merge endpoint joins inside their antialiasing region");
        if (adjustedFlatCap.size() == simplifiedFlatCap.size()) {
            for (qsizetype index = 0; index < adjustedFlatCap.size(); ++index) {
                QVERIFY2(
                    (equalVertex(adjustedFlatCap[index], simplifiedFlatCap[index])),
                    "merged flat-cap endpoint joins must preserve the simplified stroke geometry");
            }
        }

        QVector<EditorRhiSolidVertex> shortSquareCap;
        EditorRhiGeometry::appendAntialiasedStroke(shortSquareCap, shortEndpointSegments, 1.5,
                                                   QColor(255, 255, 255), 1.0, 3.0, Qt::SquareCap,
                                                   Qt::RoundJoin);
        QVector<EditorRhiSolidVertex> simplifiedSquareCap;
        EditorRhiGeometry::appendAntialiasedStroke(simplifiedSquareCap, simplifiedEndpointSegments,
                                                   1.5, QColor(255, 255, 255), 1.0, 3.0,
                                                   Qt::SquareCap, Qt::RoundJoin);
        QVERIFY2((shortSquareCap.size() > simplifiedSquareCap.size()),
                 "square caps must preserve endpoint joins outside their antialiasing region");
    }

    void subpixelOverlayCoverage() {
        const QRectF clipRect(0.0, 0.0, 10.0, 10.0);
        const QColor overlayColor(12, 34, 56, 192);
        QVector<EditorRhiOverlayRect> overlayRects;
        EditorRhiGeometry::appendAntialiasedVerticalOverlay(overlayRects, 4.25, 2.0, 12.0, 1.0,
                                                            overlayColor);
        QVERIFY2((overlayRects.size() == 2),
                 "a subpixel vertical overlay must split coverage across adjacent pixels");
        if (overlayRects.size() == 2) {
            QVERIFY2((overlayRects[0].physicalViewportRect == QRectF(3.0, 2.0, 1.0, 10.0) &&
                      overlayRects[0].color == overlayColor && overlayRects[0].coverage == 0.25f),
                     "the leading overlay pixel must preserve its rectangle, color, and coverage");
            QVERIFY2((overlayRects[1].physicalViewportRect == QRectF(4.0, 2.0, 1.0, 10.0) &&
                      overlayRects[1].color == overlayColor && overlayRects[1].coverage == 0.75f),
                     "the trailing overlay pixel must preserve its rectangle, color, and coverage");
        }
        QVector<EditorRhiOverlayRect> hiddenOverlayRects;
        EditorRhiGeometry::appendAntialiasedVerticalOverlay(hiddenOverlayRects, 4.25, 2.0, 12.0,
                                                            1.0, Qt::transparent);
        QVERIFY2((hiddenOverlayRects.isEmpty()),
                 "transparent overlays must not produce draw rectangles");
    }

    void antialiasedCircle() {
        const QRectF clipRect(0.0, 0.0, 10.0, 10.0);
        QVector<EditorRhiSolidVertex> circle;
        const QPointF circleCenter(4.0, 5.0);
        EditorRhiGeometry::appendAntialiasedCircle(circle, circleCenter, 3.0,
                                                   QColor(255, 255, 255));
        QVERIFY2((!circle.isEmpty() && circle.size() % 3 == 0),
                 "a circle must produce complete antialiased triangles");
        for (qsizetype index = 0; index + 2 < circle.size(); index += 3) {
            QVERIFY2(
                (twiceTriangleArea(circle[index], circle[index + 1], circle[index + 2]) > 1e-6),
                "circle geometry must not contain degenerate triangles");
        }
        for (const auto &value : circle) {
            QVERIFY2((std::hypot(value.x - circleCenter.x(), value.y - circleCenter.y()) <= 3.7501),
                     "circle vertices must stay inside the feathered radius");
            QVERIFY2((value.coverage == 0.0f || value.coverage == 1.0f),
                     "circle coverage must preserve the opaque core and transparent feather edge");
        }
    }
};

QTEST_GUILESS_MAIN(EditorRhiGeometryTests)
#include "main.moc"
