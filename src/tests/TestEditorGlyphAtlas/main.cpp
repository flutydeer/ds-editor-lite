#include "UI/Views/Common/EditorGlyphAtlas.h"

#include <QApplication>
#include <QFontMetricsF>
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
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(12);
    const QFontMetricsF metrics(font);
    QString text;
    while (std::ceil(metrics.horizontalAdvance(text)) + 2 < 34)
        text.append(QLatin1Char('M'));
    const auto distinctText = text + QChar(0x200b);

    EditorGlyphAtlas atlas(QSize(64, 64), 1);
    atlas.beginFrame();
    atlas.appendText(text, font, QPointF(0, 0), Qt::white);
    atlas.appendText(distinctText, font, QPointF(0, 20), Qt::white);

    const auto batches = atlas.textureBatches();
    expect(batches.size() == 1, "two small blocks must share one atlas page");
    if (batches.size() == 1) {
        const auto &vertices = batches.front().vertices;
        expect(vertices.size() == 12, "both text blocks must emit one quad");
        for (const auto &vertex : vertices) {
            expect(vertex.u >= 0.0f && vertex.u <= 1.0f,
                   "horizontal texture coordinates must stay inside the atlas");
            expect(vertex.v >= 0.0f && vertex.v <= 1.0f,
                   "vertical texture coordinates must stay inside the atlas");
        }
        if (vertices.size() == 12)
            expect(vertices[6].v > vertices[0].v,
                   "the second block must advance to the next atlas row");
    }

    if (g_failures == 0) {
        QTextStream(stdout) << "All EditorGlyphAtlas tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
