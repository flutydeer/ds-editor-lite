#ifndef EDITORGLYPHATLAS_H
#define EDITORGLYPHATLAS_H

#include "EditorRhiWidget.h"

#include <QFont>
#include <QHash>
#include <QRawFont>
#include <QRectF>

#include <memory>
#include <vector>

class EditorGlyphAtlas final {
public:
    explicit EditorGlyphAtlas(QSize pageSize = QSize(1024, 1024), int maximumPages = 4);

    void beginFrame();
    void clear();
    void appendText(const QString &text, const QFont &physicalFont, const QPointF &physicalTopLeft,
                    const QColor &color, const QRectF &physicalClip = {});
    [[nodiscard]] QVector<EditorRhiTextureBatch> textureBatches() const;
    [[nodiscard]] double hitRate() const;

private:
    struct GlyphEntry {
        int pageId = -1;
        QRect rect;
        QPointF bearing;
        quint64 lastUse = 0;
    };

    struct Page {
        int id = -1;
        QImage image;
        int cursorX = 1;
        int cursorY = 1;
        int rowHeight = 0;
        quint64 generation = 1;
        quint64 lastUse = 0;
        QVector<EditorRhiTextVertex> vertices;
    };

    GlyphEntry *ensureGlyph(const QRawFont &font, quint32 glyphIndex);
    Page *allocatePageFor(const QSize &glyphSize);
    Page *createPage();
    void clearPage(Page &page);
    [[nodiscard]] QString glyphKey(const QRawFont &font, quint32 glyphIndex) const;
    static void appendTexturedRect(QVector<EditorRhiTextVertex> &vertices, const QRectF &target,
                                   const QRectF &sourcePixels, const QSize &textureSize,
                                   const QColor &color);

    QSize m_pageSize;
    int m_maximumPages = 4;
    int m_nextPageId = 0;
    quint64 m_useCounter = 0;
    quint64 m_hitCount = 0;
    quint64 m_missCount = 0;
    std::vector<std::unique_ptr<Page>> m_pages;
    QHash<QString, GlyphEntry> m_glyphs;
};

#endif // EDITORGLYPHATLAS_H
