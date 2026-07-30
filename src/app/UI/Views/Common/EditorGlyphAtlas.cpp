#include "EditorGlyphAtlas.h"

#include <QGlyphRun>
#include <QPainter>
#include <QRawFont>
#include <QTextLayout>

#include <algorithm>

EditorGlyphAtlas::EditorGlyphAtlas(const QSize pageSize, const int maximumPages)
    : m_pageSize(pageSize.expandedTo(QSize(64, 64))), m_maximumPages(std::max(1, maximumPages)) {
}

void EditorGlyphAtlas::beginFrame() {
    ++m_useCounter;
    for (const auto &page : m_pages)
        page->vertices.clear();
}

void EditorGlyphAtlas::clear() {
    m_pages.clear();
    m_glyphs.clear();
    m_nextPageId = 0;
    m_hitCount = 0;
    m_missCount = 0;
}

void EditorGlyphAtlas::appendText(const QString &text, const QFont &physicalFont,
                                  const QPointF &physicalTopLeft, const QColor &color,
                                  const QRectF &physicalClip) {
    if (text.isEmpty() || color.alpha() == 0)
        return;

    QTextLayout layout(text, physicalFont);
    layout.beginLayout();
    const auto line = layout.createLine();
    layout.endLayout();
    if (!line.isValid())
        return;

    const auto runs = line.glyphRuns();
    for (const auto &run : runs) {
        const auto rawFont = run.rawFont();
        const auto glyphIndexes = run.glyphIndexes();
        const auto positions = run.positions();
        const auto count = std::min(glyphIndexes.size(), positions.size());
        for (qsizetype i = 0; i < count; ++i) {
            auto *entry = ensureGlyph(rawFont, glyphIndexes.at(i));
            if (!entry || entry->rect.isEmpty())
                continue;
            auto pageIterator =
                std::find_if(m_pages.begin(), m_pages.end(),
                             [entry](const auto &p) { return p->id == entry->pageId; });
            if (pageIterator == m_pages.end())
                continue;
            auto &page = **pageIterator;
            page.lastUse = m_useCounter;
            entry->lastUse = m_useCounter;

            QRectF target(physicalTopLeft + positions.at(i) + entry->bearing, entry->rect.size());
            QRectF source(entry->rect);
            if (!physicalClip.isEmpty()) {
                const auto clipped = target.intersected(physicalClip);
                if (clipped.isEmpty())
                    continue;
                const auto delta = clipped.topLeft() - target.topLeft();
                source = QRectF(source.topLeft() + delta, clipped.size());
                target = clipped;
            }
            appendTexturedRect(page.vertices, target, source, page.image.size(), color);
        }
    }
}

QVector<EditorRhiTextureBatch> EditorGlyphAtlas::textureBatches() const {
    QVector<EditorRhiTextureBatch> result;
    result.reserve(m_pages.size());
    for (const auto &page : m_pages) {
        if (page->vertices.isEmpty())
            continue;
        result.append({page->id, page->generation, page->image, page->vertices});
    }
    return result;
}

double EditorGlyphAtlas::hitRate() const {
    const auto total = m_hitCount + m_missCount;
    return total > 0 ? static_cast<double>(m_hitCount) / static_cast<double>(total) : 1.0;
}

EditorGlyphAtlas::GlyphEntry *EditorGlyphAtlas::ensureGlyph(const QRawFont &font,
                                                            const quint32 glyphIndex) {
    const auto key = glyphKey(font, glyphIndex);
    const auto existing = m_glyphs.find(key);
    if (existing != m_glyphs.end()) {
        ++m_hitCount;
        existing->lastUse = m_useCounter;
        return &existing.value();
    }
    ++m_missCount;

    auto alpha = font.alphaMapForGlyph(glyphIndex, QRawFont::PixelAntialiasing);
    if (alpha.isNull())
        return nullptr;
    if (alpha.format() != QImage::Format_Alpha8)
        alpha = alpha.convertToFormat(QImage::Format_Alpha8);

    const auto bounds = font.boundingRect(glyphIndex);
    auto *page = allocatePageFor(alpha.size());
    if (!page)
        return nullptr;
    if (page->cursorX + alpha.width() + 1 > m_pageSize.width()) {
        page->cursorX = 1;
        page->cursorY += page->rowHeight + 1;
        page->rowHeight = 0;
    }
    if (page->cursorY + alpha.height() + 1 > m_pageSize.height())
        return nullptr;

    const QRect target(page->cursorX, page->cursorY, alpha.width(), alpha.height());
    QImage rgba(alpha.size(), QImage::Format_RGBA8888_Premultiplied);
    rgba.fill(Qt::white);
    rgba.setAlphaChannel(alpha);
    QPainter painter(&page->image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(target.topLeft(), rgba);
    painter.end();
    ++page->generation;
    page->cursorX += alpha.width() + 1;
    page->rowHeight = std::max(page->rowHeight, alpha.height());
    page->lastUse = m_useCounter;

    GlyphEntry entry;
    entry.pageId = page->id;
    entry.rect = target;
    entry.bearing = bounds.topLeft();
    entry.lastUse = m_useCounter;
    const auto inserted = m_glyphs.insert(key, entry);
    return &inserted.value();
}

EditorGlyphAtlas::Page *EditorGlyphAtlas::allocatePageFor(const QSize &glyphSize) {
    if (glyphSize.width() + 2 > m_pageSize.width() || glyphSize.height() + 2 > m_pageSize.height())
        return nullptr;
    for (const auto &page : m_pages) {
        const auto fitsCurrentRow = page->cursorX + glyphSize.width() + 1 <= m_pageSize.width() &&
                                    page->cursorY + glyphSize.height() + 1 <= m_pageSize.height();
        const auto fitsNextRow =
            page->cursorY + page->rowHeight + glyphSize.height() + 2 <= m_pageSize.height();
        if (fitsCurrentRow || fitsNextRow)
            return page.get();
    }
    if (m_pages.size() < m_maximumPages)
        return createPage();

    const auto leastRecentlyUsed =
        std::min_element(m_pages.begin(), m_pages.end(), [](const auto &left, const auto &right) {
            return left->lastUse < right->lastUse;
        });
    clearPage(**leastRecentlyUsed);
    return leastRecentlyUsed->get();
}

EditorGlyphAtlas::Page *EditorGlyphAtlas::createPage() {
    auto page = std::make_unique<Page>();
    page->id = m_nextPageId++;
    page->image = QImage(m_pageSize, QImage::Format_RGBA8888_Premultiplied);
    page->image.fill(Qt::transparent);
    page->lastUse = m_useCounter;
    auto *result = page.get();
    m_pages.push_back(std::move(page));
    return result;
}

void EditorGlyphAtlas::clearPage(Page &page) {
    for (auto iterator = m_glyphs.begin(); iterator != m_glyphs.end();) {
        if (iterator->pageId == page.id)
            iterator = m_glyphs.erase(iterator);
        else
            ++iterator;
    }
    page.image.fill(Qt::transparent);
    page.cursorX = 1;
    page.cursorY = 1;
    page.rowHeight = 0;
    page.vertices.clear();
    page.lastUse = m_useCounter;
    ++page.generation;
}

QString EditorGlyphAtlas::glyphKey(const QRawFont &font, const quint32 glyphIndex) const {
    return QStringLiteral("%1\n%2\n%3\n%4")
        .arg(font.familyName(), font.styleName())
        .arg(font.pixelSize(), 0, 'f', 3)
        .arg(glyphIndex);
}

void EditorGlyphAtlas::appendTexturedRect(QVector<EditorRhiTextVertex> &vertices,
                                          const QRectF &target, const QRectF &sourcePixels,
                                          const QSize &textureSize, const QColor &color) {
    if (target.isEmpty() || sourcePixels.isEmpty() || textureSize.isEmpty())
        return;
    const auto alpha = static_cast<float>(color.alphaF());
    const auto red = static_cast<float>(color.redF()) * alpha;
    const auto green = static_cast<float>(color.greenF()) * alpha;
    const auto blue = static_cast<float>(color.blueF()) * alpha;
    const auto leftU = static_cast<float>(sourcePixels.left() / textureSize.width());
    const auto rightU = static_cast<float>(sourcePixels.right() / textureSize.width());
    const auto topV = static_cast<float>(sourcePixels.top() / textureSize.height());
    const auto bottomV = static_cast<float>(sourcePixels.bottom() / textureSize.height());
    const auto append = [&](const double x, const double y, const float u, const float v) {
        vertices.append(
            {static_cast<float>(x), static_cast<float>(y), u, v, red, green, blue, alpha});
    };
    append(target.left(), target.top(), leftU, topV);
    append(target.right(), target.top(), rightU, topV);
    append(target.right(), target.bottom(), rightU, bottomV);
    append(target.left(), target.top(), leftU, topV);
    append(target.right(), target.bottom(), rightU, bottomV);
    append(target.left(), target.bottom(), leftU, bottomV);
}
