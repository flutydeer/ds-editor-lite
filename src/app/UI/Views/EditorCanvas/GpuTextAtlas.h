#ifndef GPUTEXTATLAS_H
#define GPUTEXTATLAS_H

#include "EditorCanvasTypes.h"

#include <QHash>
#include <QImage>

class QRawFont;

struct GpuTextQuad {
    QRectF bounds;
    QRectF textureCoordinates;
    QColor color;
};

class GpuTextAtlas final {
public:
    explicit GpuTextAtlas(QSize size = {1024, 1024});

    [[nodiscard]] QVector<GpuTextQuad> layout(const QVector<EditorRenderText> &texts,
                                              double devicePixelRatio);
    [[nodiscard]] const QImage &image() const;
    [[nodiscard]] quint64 revision() const;
    [[nodiscard]] quint64 hitCount() const;
    [[nodiscard]] quint64 missCount() const;
    void reset(double devicePixelRatio);

private:
    struct GlyphEntry {
        QRect pixelRect;
        QRectF logicalBounds;
    };

    [[nodiscard]] QString glyphKey(const QRawFont &font, quint32 glyphIndex) const;
    [[nodiscard]] const GlyphEntry *ensureGlyph(const QRawFont &font, quint32 glyphIndex);
    [[nodiscard]] QVector<GpuTextQuad> layoutOnce(const QVector<EditorRenderText> &texts);

    QSize m_size;
    QImage m_image;
    QHash<QString, GlyphEntry> m_entries;
    double m_devicePixelRatio = 1.0;
    int m_cursorX = 1;
    int m_cursorY = 1;
    int m_rowHeight = 0;
    quint64 m_revision = 0;
    quint64 m_hitCount = 0;
    quint64 m_missCount = 0;
    bool m_atlasFull = false;
};

#endif // GPUTEXTATLAS_H
