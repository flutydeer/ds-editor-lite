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
        bool hasCoverage = false;
        bool hasValidPremultipliedCoverage = true;
        const auto &image = batches.front().image;
        for (int y = 0; y < image.height(); ++y) {
            const auto *scanLine = image.constScanLine(y);
            for (int x = 0; x < image.width(); ++x) {
                const auto red = scanLine[x * 4];
                const auto green = scanLine[x * 4 + 1];
                const auto blue = scanLine[x * 4 + 2];
                const auto alpha = scanLine[x * 4 + 3];
                hasCoverage = hasCoverage || alpha != 0;
                hasValidPremultipliedCoverage = hasValidPremultipliedCoverage && red <= alpha &&
                                                green <= alpha && blue <= alpha;
            }
        }
        expect(hasCoverage, "rasterized text must produce atlas coverage");
        expect(hasValidPremultipliedCoverage,
               "subpixel coverage must remain valid premultiplied RGBA data");

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

    EditorGlyphAtlas fractionalAtlas(QSize(128, 128), 1);
    fractionalAtlas.beginFrame();
    fractionalAtlas.appendText(text, font, QPointF(0, 0), Qt::white, {}, 1.25);
    const auto fractionalBatches = fractionalAtlas.textureBatches();
    expect(fractionalBatches.size() == 1 && fractionalBatches.front().vertices.size() == 6,
           "fractional DPR text must emit one atlas quad");
    if (fractionalBatches.size() == 1 && fractionalBatches.front().vertices.size() == 6) {
        const auto &vertices = fractionalBatches.front().vertices;
        const auto expectedWidth = std::ceil(metrics.horizontalAdvance(text) * 1.25);
        const auto rasterWidth = vertices[1].x - vertices[0].x;
        expect(rasterWidth >= expectedWidth && rasterWidth <= expectedWidth + 1.0,
               "fractional DPR text width must preserve layout plus subpixel overflow");

        const auto previousGeneration = fractionalBatches.front().generation;
        fractionalAtlas.clear();
        fractionalAtlas.beginFrame();
        fractionalAtlas.appendText(distinctText, font, QPointF(0, 0), Qt::white, {}, 1.25);
        const auto clearedBatches = fractionalAtlas.textureBatches();
        expect(clearedBatches.size() == 1 && clearedBatches.front().generation > previousGeneration,
               "clearing an atlas must force a replacement texture upload");
    }

    EditorGlyphAtlas phaseAtlas(QSize(128, 128), 1);
    phaseAtlas.beginFrame();
    phaseAtlas.appendText(text, font, QPointF(10.1, 10), Qt::white);
    phaseAtlas.appendText(text, font, QPointF(10.109, 10), Qt::white);
    phaseAtlas.appendText(text, font, QPointF(10.3, 10), Qt::white);
    const auto phaseBatches = phaseAtlas.textureBatches();
    expect(phaseBatches.size() == 1 && phaseBatches.front().vertices.size() == 18,
           "Qt fixed-point phases must emit all requested atlas quads");
    if (phaseBatches.size() == 1 && phaseBatches.front().vertices.size() == 18) {
        const auto &vertices = phaseBatches.front().vertices;
        expect(qFuzzyCompare(vertices[6].u, vertices[0].u) &&
                   qFuzzyCompare(vertices[6].v, vertices[0].v),
               "positions in the same Qt 26.6 bucket must reuse glyph coverage");
        expect(!qFuzzyCompare(vertices[12].u, vertices[0].u),
               "distinct Qt fixed-point phases must use distinct glyph coverage");
        expect(qFuzzyCompare(phaseAtlas.hitRate(), 1.0 / 3.0),
               "only equal Qt fixed-point phases may reuse the rasterized glyph block");
    }

    EditorGlyphAtlas cameraAtlas(QSize(128, 128), 1);
    cameraAtlas.beginFrame();
    cameraAtlas.appendText(text, font, QPointF(20.4, 30.2), Qt::white, {}, 1.0, QPointF(10, 20));
    cameraAtlas.appendText(text, font, QPointF(21.4, 31.2), Qt::white, {}, 1.0, QPointF(11, 21));
    const auto cameraBatches = cameraAtlas.textureBatches();
    expect(cameraBatches.size() == 1 && cameraBatches.front().vertices.size() == 12,
           "equal viewport phases must emit two atlas quads");
    if (cameraBatches.size() == 1 && cameraBatches.front().vertices.size() == 12) {
        const auto &vertices = cameraBatches.front().vertices;
        expect(qFuzzyCompare(vertices[6].x - vertices[0].x, 1.0f) &&
                   qFuzzyCompare(vertices[6].y - vertices[0].y, 1.0f),
               "camera movement must preserve viewport-relative glyph alignment");
        expect(qFuzzyCompare(cameraAtlas.hitRate(), 0.5),
               "equal viewport phases must reuse the rasterized glyph block");
    }

    EditorGlyphAtlas windowAtlas(QSize(128, 128), 1);
    windowAtlas.beginFrame();
    windowAtlas.appendText(text, font, QPointF(20.4, 30.2), Qt::white, {}, 1.0, QPointF(10, 20),
                           QPointF(0.5, 0));
    windowAtlas.appendText(text, font, QPointF(20.9, 30.2), Qt::white, {}, 1.0, QPointF(10, 20),
                           QPointF(0, 0));
    expect(qFuzzyCompare(windowAtlas.hitRate(), 0.5),
           "window-relative glyph positions must share coverage at the same device phase");

    EditorGlyphAtlas orderedAtlas(QSize(128, 128), 1);
    EditorRhiFrameData orderedFrame;
    orderedAtlas.beginFrame();
    orderedFrame.solidVertices.resize(6);
    const auto firstText = orderedAtlas.appendText(text, font, QPointF(0, 0), Qt::white, {}, 1.0);
    orderedFrame.drawList.appendTexture(firstText, orderedFrame.solidVertices.size());
    orderedFrame.solidVertices.resize(12);
    const auto secondText =
        orderedAtlas.appendText(distinctText, font, QPointF(0, 20), Qt::white, {}, 1.0);
    orderedFrame.drawList.appendTexture(secondText, orderedFrame.solidVertices.size());
    orderedFrame.drawList.finish(orderedFrame.solidVertices.size());
    const auto &commands = orderedFrame.drawList.commands;
    expect(commands.size() == 4, "solid and text draws must preserve their append order");
    if (commands.size() == 4) {
        expect(commands[0].type == EditorRhiDrawCommand::Type::Solid &&
                   commands[0].vertexOffset == 0 && commands[0].vertexCount == 6 &&
                   commands[1].type == EditorRhiDrawCommand::Type::Texture &&
                   commands[2].type == EditorRhiDrawCommand::Type::Solid &&
                   commands[2].vertexOffset == 6 && commands[2].vertexCount == 6 &&
                   commands[3].type == EditorRhiDrawCommand::Type::Texture,
               "later solid geometry must be able to occlude earlier text");
    }

    EditorGlyphAtlas coloredAtlas(QSize(128, 128), 1);
    EditorRhiDrawList coloredDrawList;
    coloredAtlas.beginFrame();
    const auto whiteText = coloredAtlas.appendText(text, font, QPointF(0, 0), Qt::white, {}, 1.0);
    const auto blackText = coloredAtlas.appendText(text, font, QPointF(0, 20), Qt::black, {}, 1.0);
    coloredDrawList.appendTexture(whiteText, 0);
    coloredDrawList.appendTexture(blackText, 0);
    expect(coloredDrawList.commands.size() == 2,
           "adjacent text draws with different blend colors must remain separate");
    if (coloredDrawList.commands.size() == 2) {
        expect(coloredDrawList.commands[0].color == Qt::white &&
                   coloredDrawList.commands[1].color == Qt::black,
               "text draw commands must preserve their blend colors");
    }

    EditorGlyphAtlas saturatedAtlas(QSize(64, 64), 1);
    saturatedAtlas.beginFrame();
    qsizetype previousVertexCount = 0;
    for (int i = 0; i < 100; ++i) {
        saturatedAtlas.appendText(QString::number(i), font, QPointF(0, i * 20), Qt::white);
        qsizetype vertexCount = 0;
        for (const auto &batch : saturatedAtlas.textureBatches())
            vertexCount += batch.vertices.size();
        expect(vertexCount >= previousVertexCount,
               "an atlas page referenced by the current frame must not be evicted");
        previousVertexCount = vertexCount;
    }

    if (g_failures == 0) {
        QTextStream(stdout) << "All EditorGlyphAtlas tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
