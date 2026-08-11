#ifndef EDITORGLYPHATLAS_H
#define EDITORGLYPHATLAS_H

#include "EditorRhiWidget.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

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
    struct Block {
        int pageId = -1;
        QRect rect;        // 页内分配区域（含 padding），坐标在页内像素网格上
        QRect contentRect; // 页内实际文本内容区域（不含 padding）
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

    Block *ensureBlock(const QFont &font, const QString &text);
    Page *allocatePageFor(const QSize &blockSize);
    Page *createPage();
    void clearPage(Page &page);
    [[nodiscard]] QString blockKey(const QFont &font, const QString &text) const;
    [[nodiscard]] static QSize measureTextPixels(const QFont &font, const QString &text,
                                                 int padding);
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
    QHash<QString, Block> m_blocks;
};

#endif // EDITORGLYPHATLAS_H
