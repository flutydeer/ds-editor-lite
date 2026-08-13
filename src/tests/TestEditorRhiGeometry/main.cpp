#include "UI/Views/Common/EditorRhiGeometry.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cmath>

namespace {
    int g_failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }

    bool equalVertex(const EditorRhiSolidVertex &lhs, const EditorRhiSolidVertex &rhs) {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.r == rhs.r && lhs.g == rhs.g &&
               lhs.b == rhs.b && lhs.a == rhs.a && lhs.coverage == rhs.coverage;
    }

    double twiceTriangleArea(const EditorRhiSolidVertex &a, const EditorRhiSolidVertex &b,
                             const EditorRhiSolidVertex &c) {
        return std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QRectF clipRect(0.0, 0.0, 10.0, 10.0);

    const QVector<EditorRhiSolidVertex> inside{
        {1.0f, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
        {8.0f, 1.0f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f},
        {1.0f, 8.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f},
    };
    QVector<EditorRhiSolidVertex> accepted;
    EditorRhiGeometry::appendClippedTriangles(accepted, inside, clipRect);
    expect(accepted.size() == inside.size(), "fully visible triangles must be appended intact");
    if (accepted.size() == inside.size()) {
        for (qsizetype index = 0; index < inside.size(); ++index)
            expect(equalVertex(accepted[index], inside[index]),
                   "the clipping fast path must preserve every vertex attribute");
    }

    const QVector<EditorRhiSolidVertex> outside{
        {-8.0f, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
        {-2.0f, 1.0f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f},
        {-3.0f, 8.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f},
    };
    QVector<EditorRhiSolidVertex> rejected;
    EditorRhiGeometry::appendClippedTriangles(rejected, outside, clipRect);
    expect(rejected.isEmpty(), "fully hidden triangles must be rejected as a batch");

    const QVector<EditorRhiSolidVertex> intersecting{
        {-2.0f, 2.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
        {5.0f,  2.0f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f},
        {2.0f,  8.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f},
    };
    QVector<EditorRhiSolidVertex> clipped;
    EditorRhiGeometry::appendClippedTriangles(clipped, intersecting, clipRect);
    expect(clipped.size() == 6, "a triangle crossing one clip edge must become two triangles");
    for (const auto &value : clipped) {
        expect(clipRect.contains(QPointF(value.x, value.y)),
               "partially clipped vertices must stay inside the clip rectangle");
        expect(value.coverage >= 0.5f && value.coverage <= 0.7f,
               "clipping must interpolate coverage without leaving the source range");
    }

    QVector<EditorRhiSolidVertex> hairline;
    EditorRhiGeometry::appendAntialiasedHairline(hairline,
                                                 {
                                                     {2.0, 5.0},
                                                     {8.0, 5.0}
    },
                                                 QColor(255, 255, 255));
    expect(hairline.size() == 72,
           "a single hairline segment must omit zero-area stroke and cap triangles");
    for (qsizetype index = 0; index + 2 < hairline.size(); index += 3) {
        expect(twiceTriangleArea(hairline[index], hairline[index + 1], hairline[index + 2]) > 1e-6,
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
    expect(joinedHairline.size() == 84,
           "a joined hairline must contain four triangles per segment and two round caps");
    for (qsizetype index = 0; index + 2 < joinedHairline.size(); index += 3) {
        expect(twiceTriangleArea(joinedHairline[index], joinedHairline[index + 1],
                                 joinedHairline[index + 2]) > 1e-6,
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
    expect(deduplicatedHairline.size() == hairline.size(),
           "duplicate hairline points must be removed before tessellation");
    if (deduplicatedHairline.size() == hairline.size()) {
        for (qsizetype index = 0; index < hairline.size(); ++index) {
            expect(equalVertex(deduplicatedHairline[index], hairline[index]),
                   "duplicate removal must preserve the generated hairline vertices");
        }
    }

    QVector<EditorRhiSolidVertex> circle;
    const QPointF circleCenter(4.0, 5.0);
    EditorRhiGeometry::appendAntialiasedCircle(circle, circleCenter, 3.0, QColor(255, 255, 255));
    expect(circle.size() == 288, "a circle must contain 32 non-degenerate antialiased segments");
    for (qsizetype index = 0; index + 2 < circle.size(); index += 3) {
        expect(twiceTriangleArea(circle[index], circle[index + 1], circle[index + 2]) > 1e-6,
               "circle geometry must not contain degenerate triangles");
    }
    for (const auto &value : circle) {
        expect(std::hypot(value.x - circleCenter.x(), value.y - circleCenter.y()) <= 3.7501,
               "circle vertices must stay inside the feathered radius");
        expect(value.coverage == 0.0f || value.coverage == 1.0f,
               "circle coverage must preserve the opaque core and transparent feather edge");
    }

    if (g_failures == 0) {
        QTextStream(stdout) << "All EditorRhiGeometry tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
