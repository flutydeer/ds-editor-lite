#include "GpuTextAtlas.h"

#include <QGlyphRun>
#include <QRawFont>
#include <QTextLayout>
#include <QTransform>

#include <cmath>

GpuTextAtlas::GpuTextAtlas(const QSize size) : m_size(size) {
    reset(1.0);
}

QVector<GpuTextQuad> GpuTextAtlas::layout(const QVector<EditorRenderText> &texts,
                                          const double devicePixelRatio) {
    if (!qFuzzyCompare(m_devicePixelRatio, devicePixelRatio))
        reset(devicePixelRatio);

    m_atlasFull = false;
    auto quads = layoutOnce(texts);
    if (!m_atlasFull)
        return quads;

    // A generation reset is the eviction policy. Re-shape the complete visible text set so no
    // quad keeps texture coordinates into the evicted generation.
    reset(devicePixelRatio);
    m_atlasFull = false;
    return layoutOnce(texts);
}

QVector<GpuTextQuad> GpuTextAtlas::layoutOnce(const QVector<EditorRenderText> &texts) {
    QVector<GpuTextQuad> quads;
    for (const auto &text : texts) {
        if (text.text.isEmpty() || text.color.alpha() == 0)
            continue;
        QTextLayout layout(text.text, text.font);
        layout.beginLayout();
        auto line = layout.createLine();
        if (line.isValid()) {
            line.setLineWidth(100000.0);
            line.setPosition({});
        }
        layout.endLayout();
        for (const auto &run : layout.glyphRuns()) {
            const auto font = run.rawFont();
            const auto glyphs = run.glyphIndexes();
            const auto positions = run.positions();
            const auto count = qMin(glyphs.size(), positions.size());
            for (qsizetype i = 0; i < count; ++i) {
                const auto entry = ensureGlyph(font, glyphs.at(i));
                if (!entry || entry->pixelRect.isEmpty())
                    continue;
                const auto logicalSize = QSizeF(entry->pixelRect.width() / m_devicePixelRatio,
                                                entry->pixelRect.height() / m_devicePixelRatio);
                const auto topLeft =
                    text.baseline + positions.at(i) + entry->logicalBounds.topLeft();
                const QRectF glyphBounds(topLeft, logicalSize);
                const auto bounds =
                    text.clip.isNull() ? glyphBounds : glyphBounds.intersected(text.clip);
                if (bounds.isEmpty())
                    continue;
                const auto &pixelRect = entry->pixelRect;
                const QRectF fullUv(pixelRect.x() / static_cast<double>(m_size.width()),
                                    pixelRect.y() / static_cast<double>(m_size.height()),
                                    pixelRect.width() / static_cast<double>(m_size.width()),
                                    pixelRect.height() / static_cast<double>(m_size.height()));
                const auto xRatio = glyphBounds.width() > 0.0
                                        ? (bounds.left() - glyphBounds.left()) / glyphBounds.width()
                                        : 0.0;
                const auto yRatio = glyphBounds.height() > 0.0
                                        ? (bounds.top() - glyphBounds.top()) / glyphBounds.height()
                                        : 0.0;
                const auto widthRatio =
                    glyphBounds.width() > 0.0 ? bounds.width() / glyphBounds.width() : 0.0;
                const auto heightRatio =
                    glyphBounds.height() > 0.0 ? bounds.height() / glyphBounds.height() : 0.0;
                quads.append({
                    .bounds = bounds,
                    .textureCoordinates =
                        {
                                             fullUv.left() + fullUv.width() * xRatio,
                                             fullUv.top() + fullUv.height() * yRatio,
                                             fullUv.width() * widthRatio,
                                             fullUv.height() * heightRatio,
                                             },
                    .color = text.color,
                });
            }
        }
    }
    return quads;
}

const QImage &GpuTextAtlas::image() const {
    return m_image;
}

quint64 GpuTextAtlas::revision() const {
    return m_revision;
}

quint64 GpuTextAtlas::hitCount() const {
    return m_hitCount;
}

quint64 GpuTextAtlas::missCount() const {
    return m_missCount;
}

void GpuTextAtlas::reset(const double devicePixelRatio) {
    m_devicePixelRatio = qMax(1.0, devicePixelRatio);
    m_image = QImage(m_size, QImage::Format_RGBA8888);
    m_image.fill(Qt::transparent);
    m_entries.clear();
    m_cursorX = 1;
    m_cursorY = 1;
    m_rowHeight = 0;
    ++m_revision;
}

QString GpuTextAtlas::glyphKey(const QRawFont &font, const quint32 glyphIndex) const {
    return QStringLiteral("%1\n%2\n%3\n%4\n%5\n%6\n%7\npixel-aa")
        .arg(font.familyName(), font.styleName())
        .arg(qRound64(font.pixelSize() * 1024.0))
        .arg(font.weight())
        .arg(static_cast<int>(font.style()))
        .arg(glyphIndex)
        .arg(qRound64(m_devicePixelRatio * 1024.0));
}

const GpuTextAtlas::GlyphEntry *GpuTextAtlas::ensureGlyph(const QRawFont &font,
                                                          const quint32 glyphIndex) {
    const auto key = glyphKey(font, glyphIndex);
    if (const auto it = m_entries.constFind(key); it != m_entries.cend()) {
        ++m_hitCount;
        return &it.value();
    }

    ++m_missCount;
    auto alpha =
        font.alphaMapForGlyph(glyphIndex, QRawFont::PixelAntialiasing,
                              QTransform::fromScale(m_devicePixelRatio, m_devicePixelRatio));
    if (alpha.isNull()) {
        const auto it = m_entries.insert(key, {});
        return &it.value();
    }
    alpha = alpha.convertToFormat(QImage::Format_Alpha8);
    constexpr int padding = 1;
    if (m_cursorX + alpha.width() + padding >= m_size.width()) {
        m_cursorX = padding;
        m_cursorY += m_rowHeight + padding;
        m_rowHeight = 0;
    }
    if (m_cursorY + alpha.height() + padding >= m_size.height()) {
        m_atlasFull = true;
        return nullptr;
    }

    const QRect pixelRect(m_cursorX, m_cursorY, alpha.width(), alpha.height());
    for (int y = 0; y < alpha.height(); ++y) {
        const auto *source = alpha.constScanLine(y);
        for (int x = 0; x < alpha.width(); ++x) {
            const auto coverage = source[x];
            m_image.setPixelColor(m_cursorX + x, m_cursorY + y, QColor(255, 255, 255, coverage));
        }
    }
    m_cursorX += alpha.width() + padding;
    m_rowHeight = qMax(m_rowHeight, alpha.height());
    ++m_revision;
    const auto it = m_entries.insert(
        key, {.pixelRect = pixelRect, .logicalBounds = font.boundingRect(glyphIndex)});
    return &it.value();
}
