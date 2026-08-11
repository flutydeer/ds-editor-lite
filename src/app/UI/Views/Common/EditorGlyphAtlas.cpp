#include "EditorGlyphAtlas.h"

#include <QFontInfo>
#include <QPainter>
#include <QTextOption>

#include <algorithm>
#include <cmath>

namespace {
    constexpr int kBlockPadding = 1;
}

QSize EditorGlyphAtlas::measureTextPixels(const QFont &font, const QString &text,
                                          const int padding) {
    QFontMetricsF metrics(font);
    const auto width = static_cast<int>(std::ceil(metrics.horizontalAdvance(text)));
    const auto height = static_cast<int>(std::ceil(metrics.height()));
    return QSize(width + 2 * padding, height + 2 * padding);
}

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
    m_blocks.clear();
    m_nextPageId = 0;
    m_hitCount = 0;
    m_missCount = 0;
}

void EditorGlyphAtlas::appendText(const QString &text, const QFont &physicalFont,
                                  const QPointF &physicalTopLeft, const QColor &color,
                                  const QRectF &physicalClip) {
    if (text.isEmpty() || color.alpha() == 0)
        return;

    auto *block = ensureBlock(physicalFont, text);
    if (!block || block->rect.isEmpty())
        return;
    auto pageIterator = std::find_if(m_pages.begin(), m_pages.end(),
                                     [block](const auto &p) { return p->id == block->pageId; });
    if (pageIterator == m_pages.end())
        return;
    auto &page = **pageIterator;
    page.lastUse = m_useCounter;
    block->lastUse = m_useCounter;

    // Snap the quad to the device-pixel grid so each source texel maps 1:1 to a
    // destination pixel. Combined with a Linear sampler this removes the
    // interpolation blur the old fractional glyph quads produced.
    const QPointF snappedTopLeft(std::round(physicalTopLeft.x()), std::round(physicalTopLeft.y()));
    QRectF target(snappedTopLeft, block->contentRect.size());
    QRectF source(block->contentRect);
    if (!physicalClip.isEmpty()) {
        const auto clipped = target.intersected(physicalClip);
        if (clipped.isEmpty())
            return;
        const auto delta = clipped.topLeft() - target.topLeft();
        source = QRectF(source.topLeft() + delta, clipped.size());
        target = clipped;
    }
    appendTexturedRect(page.vertices, target, source, page.image.size(), color);
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

EditorGlyphAtlas::Block *EditorGlyphAtlas::ensureBlock(const QFont &font, const QString &text) {
    const auto key = blockKey(font, text);
    const auto existing = m_blocks.find(key);
    if (existing != m_blocks.end()) {
        ++m_hitCount;
        existing->lastUse = m_useCounter;
        return &existing.value();
    }
    ++m_missCount;

    const auto blockSize = measureTextPixels(font, text, kBlockPadding);
    if (blockSize.width() + 2 > m_pageSize.width() || blockSize.width() > m_pageSize.width() ||
        blockSize.height() > m_pageSize.height())
        return nullptr;
    auto *page = allocatePageFor(blockSize);
    if (!page)
        return nullptr;

    const QRect targetBlock(page->cursorX, page->cursorY, blockSize.width(), blockSize.height());
    const auto contentRect =
        targetBlock.adjusted(kBlockPadding, kBlockPadding, -kBlockPadding, -kBlockPadding);
    const QColor white(Qt::white);
    {
        QPainter painter(&page->image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(targetBlock, QColor(0, 0, 0, 0));
        painter.setPen(white);
        painter.setFont(font);
        const QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
        painter.drawText(QRectF(contentRect), text, option);
    }
    ++page->generation;
    page->cursorX += blockSize.width() + 1;
    page->rowHeight = std::max(page->rowHeight, blockSize.height());
    page->lastUse = m_useCounter;

    Block block;
    block.pageId = page->id;
    block.rect = targetBlock;
    block.contentRect = contentRect;
    block.lastUse = m_useCounter;
    const auto inserted = m_blocks.insert(key, block);
    return &inserted.value();
}

EditorGlyphAtlas::Page *EditorGlyphAtlas::allocatePageFor(const QSize &blockSize) {
    for (const auto &page : m_pages) {
        const auto fitsCurrentRow = page->cursorX + blockSize.width() + 1 <= m_pageSize.width() &&
                                    page->cursorY + blockSize.height() + 1 <= m_pageSize.height();
        const auto fitsNextRow =
            page->cursorY + page->rowHeight + blockSize.height() + 2 <= m_pageSize.height();
        if (fitsCurrentRow)
            return page.get();
        if (fitsNextRow) {
            page->cursorX = 1;
            page->cursorY += page->rowHeight + 1;
            page->rowHeight = 0;
            return page.get();
        }
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
    for (auto iterator = m_blocks.begin(); iterator != m_blocks.end();) {
        if (iterator->pageId == page.id)
            iterator = m_blocks.erase(iterator);
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

QString EditorGlyphAtlas::blockKey(const QFont &font, const QString &text) const {
    const auto info = QFontInfo(font);
    return QStringLiteral("%1\n%2\n%3\n%4")
        .arg(info.family(), info.styleName())
        .arg(static_cast<double>(font.pixelSize()), 0, 'f', 3)
        .arg(text);
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
