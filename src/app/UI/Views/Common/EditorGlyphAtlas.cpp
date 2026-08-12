#include "EditorGlyphAtlas.h"

#include <QFontInfo>
#include <QPainter>
#include <QTextOption>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    constexpr int kBlockPadding = 1;
    constexpr double kQtFixedPointScale = 64.0;

    double qtFixedPointPhase(const double value) {
        // QPainter converts glyph positions with QFixed::fromReal, a signed 26.6
        // fixed-point conversion that truncates toward zero.
        const auto fixedValue = std::trunc(value * kQtFixedPointScale) / kQtFixedPointScale;
        return fixedValue - std::floor(fixedValue);
    }
}

QSize EditorGlyphAtlas::measureTextPixels(const QFont &font, const QString &text, const int padding,
                                          const double devicePixelRatio) {
    QFontMetricsF metrics(font);
    const auto width =
        static_cast<int>(std::ceil(metrics.horizontalAdvance(text) * devicePixelRatio));
    const auto height = static_cast<int>(std::ceil(metrics.height() * devicePixelRatio));
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

EditorRhiTextureDrawSpan EditorGlyphAtlas::appendText(
    const QString &text, const QFont &logicalFont, const QPointF &physicalTopLeft,
    const QColor &color, const QRectF &physicalClip, const double devicePixelRatio,
    const QPointF &physicalCameraOffset, const QPointF &physicalWindowOffset) {
    if (text.isEmpty() || color.alpha() == 0 || !std::isfinite(devicePixelRatio) ||
        devicePixelRatio <= 0.0) {
        return {};
    }

    const auto viewportTopLeft = physicalTopLeft - physicalCameraOffset;
    // QPainter chooses glyph coverage from the final device position. Include the widget's
    // physical window origin for that phase, while keeping the RHI quad in viewport coordinates.
    const auto windowTopLeft = viewportTopLeft + physicalWindowOffset;
    const QPointF physicalPhase(qtFixedPointPhase(windowTopLeft.x()),
                                qtFixedPointPhase(windowTopLeft.y()));
    const QPointF alignedViewportTopLeft(std::floor(viewportTopLeft.x()),
                                         std::floor(viewportTopLeft.y()));
    auto *block = ensureBlock(logicalFont, text, devicePixelRatio, physicalPhase);
    if (!block || block->rect.isEmpty())
        return {};
    auto pageIterator = std::find_if(m_pages.begin(), m_pages.end(),
                                     [block](const auto &p) { return p->id == block->pageId; });
    if (pageIterator == m_pages.end())
        return {};
    auto &page = **pageIterator;
    page.lastUse = m_useCounter;
    block->lastUse = m_useCounter;

    QRectF target(alignedViewportTopLeft + physicalCameraOffset, block->contentRect.size());
    QRectF source(block->contentRect);
    if (!physicalClip.isEmpty()) {
        const auto clipped = target.intersected(physicalClip);
        if (clipped.isEmpty())
            return {};
        const auto delta = clipped.topLeft() - target.topLeft();
        source = QRectF(source.topLeft() + delta, clipped.size());
        target = clipped;
    }
    const auto vertexOffset = page.vertices.size();
    appendTexturedRect(page.vertices, target, source, page.image.size(), color);
    return {page.id, vertexOffset, page.vertices.size() - vertexOffset, color};
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

EditorGlyphAtlas::Block *EditorGlyphAtlas::ensureBlock(const QFont &font, const QString &text,
                                                       const double devicePixelRatio,
                                                       const QPointF &physicalPhase) {
    const auto key = blockKey(font, text, devicePixelRatio, physicalPhase);
    const auto existing = m_blocks.find(key);
    if (existing != m_blocks.end()) {
        ++m_hitCount;
        existing->lastUse = m_useCounter;
        return &existing.value();
    }
    ++m_missCount;

    auto blockSize = measureTextPixels(font, text, kBlockPadding, devicePixelRatio);
    const auto phaseWidth = qFuzzyIsNull(physicalPhase.x()) ? 0 : 1;
    const auto phaseHeight = qFuzzyIsNull(physicalPhase.y()) ? 0 : 1;
    blockSize += QSize(phaseWidth, phaseHeight);
    if (blockSize.width() + 2 > m_pageSize.width() || blockSize.height() + 2 > m_pageSize.height())
        return nullptr;
    auto *page = allocatePageFor(blockSize);
    if (!page)
        return nullptr;

    const QRect targetBlock(page->cursorX, page->cursorY, blockSize.width(), blockSize.height());
    const auto contentRect =
        targetBlock.adjusted(kBlockPadding, kBlockPadding, -kBlockPadding, -kBlockPadding);
    QImage rasterizedText(blockSize, QImage::Format_RGB32);
    rasterizedText.setDevicePixelRatio(devicePixelRatio);
    rasterizedText.fill(Qt::white);
    {
        QPainter painter(&rasterizedText);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setPen(Qt::black);
        painter.setFont(font);
        const QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
        painter.drawText(QRectF((kBlockPadding + physicalPhase.x()) / devicePixelRatio,
                                (kBlockPadding + physicalPhase.y()) / devicePixelRatio,
                                (contentRect.width() - phaseWidth) / devicePixelRatio,
                                (contentRect.height() - phaseHeight) / devicePixelRatio),
                         text, option);
    }
    QImage coverage(blockSize, QImage::Format_RGBA8888_Premultiplied);
    coverage.fill(Qt::transparent);
    for (int y = 0; y < blockSize.height(); ++y) {
        auto *target = coverage.scanLine(y);
        for (int x = 0; x < blockSize.width(); ++x) {
            const auto source = rasterizedText.pixel(x, y);
            const auto red = 255 - qRed(source);
            const auto green = 255 - qGreen(source);
            const auto blue = 255 - qBlue(source);
            const auto alpha = std::max({red, green, blue});
            target[x * 4] = static_cast<uchar>(red);
            target[x * 4 + 1] = static_cast<uchar>(green);
            target[x * 4 + 2] = static_cast<uchar>(blue);
            target[x * 4 + 3] = static_cast<uchar>(alpha);
        }
    }
    {
        QPainter painter(&page->image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(targetBlock.topLeft(), coverage);
    }
    page->generation = ++m_generationCounter;
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

    const auto leastRecentlyUsed = std::min_element(
        m_pages.begin(), m_pages.end(), [this](const auto &left, const auto &right) {
            const auto leftUse =
                left->lastUse == m_useCounter ? std::numeric_limits<quint64>::max() : left->lastUse;
            const auto rightUse = right->lastUse == m_useCounter
                                      ? std::numeric_limits<quint64>::max()
                                      : right->lastUse;
            return leftUse < rightUse;
        });
    if (leastRecentlyUsed == m_pages.end() || (*leastRecentlyUsed)->lastUse == m_useCounter)
        return nullptr;
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
    page.generation = ++m_generationCounter;
}

QString EditorGlyphAtlas::blockKey(const QFont &font, const QString &text,
                                   const double devicePixelRatio,
                                   const QPointF &physicalPhase) const {
    const auto info = QFontInfo(font);
    return QStringLiteral("%1\n%2\n%3\n%4\n%5\n%6\n%7")
        .arg(info.family(), info.styleName(), font.toString())
        .arg(devicePixelRatio, 0, 'f', 6)
        .arg(physicalPhase.x(), 0, 'f', 6)
        .arg(physicalPhase.y(), 0, 'f', 6)
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
