#include "GhostNoteLayer.h"

#include <algorithm>

namespace {
    // Emission window margins around the visible range. Scrolling inside them reuses the
    // retained vertices; crossing them re-emits around the new visible range. Kept small
    // on purpose: every frame splices and uploads the whole window, so a wide margin
    // trades saved re-emissions for permanently heavier frames.
    constexpr double horizontalMarginInViews = 0.25;
    constexpr double verticalMarginInViews = 0.25;

    void appendRect(QVector<EditorRhiSolidVertex> &vertices, const QRectF &logical,
                    const QColor &color, const double dpr) {
        if (logical.right() <= logical.left() || logical.bottom() <= logical.top() ||
            color.alpha() == 0)
            return;
        const auto alpha = static_cast<float>(color.alphaF());
        const auto r = static_cast<float>(color.redF()) * alpha;
        const auto g = static_cast<float>(color.greenF()) * alpha;
        const auto b = static_cast<float>(color.blueF()) * alpha;
        const auto left = static_cast<float>(logical.left() * dpr);
        const auto top = static_cast<float>(logical.top() * dpr);
        const auto right = static_cast<float>(logical.right() * dpr);
        const auto bottom = static_cast<float>(logical.bottom() * dpr);
        vertices.append({left, top, r, g, b, alpha, 1.0f});
        vertices.append({right, top, r, g, b, alpha, 1.0f});
        vertices.append({right, bottom, r, g, b, alpha, 1.0f});
        vertices.append({left, top, r, g, b, alpha, 1.0f});
        vertices.append({right, bottom, r, g, b, alpha, 1.0f});
        vertices.append({left, bottom, r, g, b, alpha, 1.0f});
    }
} // namespace

void GhostNoteLayer::markDirty() {
    m_dirty = true;
}

const QVector<EditorRhiSolidVertex> &GhostNoteLayer::ensureUpToDate(
    const GhostNoteSource *source, const double clipStart, const double xAtZeroTick,
    const double pixelsPerTick, const double rowHeight, const double dpr, const double localStart,
    const double localEnd, const double sceneTop, const double sceneBottom) {
    const auto enabled = source && source->enabled();
    if (!m_dirty && enabled == m_enabled && m_dpr == dpr && m_pixelsPerTick == pixelsPerTick &&
        m_rowHeight == rowHeight && m_clipStart == clipStart && m_xAtZeroTick == xAtZeroTick &&
        localStart >= m_fromTick && localEnd <= m_toTick && sceneTop >= m_bandTop &&
        sceneBottom <= m_bandBottom)
        return m_vertices;

    const auto viewTicks = std::max(0.0, localEnd - localStart);
    const auto viewHeight = std::max(0.0, sceneBottom - sceneTop);
    rebuild(source, clipStart, xAtZeroTick, pixelsPerTick, rowHeight, dpr,
            localStart - viewTicks * horizontalMarginInViews,
            localEnd + viewTicks * horizontalMarginInViews,
            sceneTop - viewHeight * verticalMarginInViews,
            sceneBottom + viewHeight * verticalMarginInViews);
    return m_vertices;
}

void GhostNoteLayer::rebuild(const GhostNoteSource *source, const double clipStart,
                             const double xAtZeroTick, const double pixelsPerTick,
                             const double rowHeight, const double dpr, const double from,
                             const double to, const double bandTop, const double bandBottom) {
    m_dirty = false;
    m_enabled = source && source->enabled();
    m_dpr = dpr;
    m_pixelsPerTick = pixelsPerTick;
    m_rowHeight = rowHeight;
    m_clipStart = clipStart;
    m_xAtZeroTick = xAtZeroTick;
    m_fromTick = from;
    m_toTick = to;
    m_bandTop = bandTop;
    m_bandBottom = bandBottom;
    m_vertices.clear();
    if (!m_enabled)
        return;
    const auto &ghosts = source->notes();
    if (ghosts.isEmpty())
        return;

    const auto barHeight =
        std::max(GhostNoteStyle::minHeight, rowHeight * GhostNoteStyle::heightRatio);
    // Notes have length, so one starting left of the window can still reach into it.
    // Begin the scan at from + clipStart - maxLength.
    const auto scanFrom = from + clipStart - source->maxLength();
    const auto first = std::lower_bound(
        ghosts.begin(), ghosts.end(), scanFrom,
        [](const GhostNote &note, const double tick) { return note.globalStart < tick; });

    const GhostNoteStyle::ColorTable fillColors;
    for (auto it = first; it != ghosts.end(); ++it) {
        const auto ghostStart = it->globalStart - clipStart;
        if (ghostStart > to)
            break; // the list is sorted by globalStart
        if (ghostStart + it->length < from)
            continue;
        const auto top = (127 - it->keyIndex) * rowHeight + (rowHeight - barHeight) * 0.5;
        if (top + barHeight < bandTop || top > bandBottom)
            continue;
        const auto left = ghostStart * pixelsPerTick + xAtZeroTick;
        const auto right = left + it->length * pixelsPerTick;
        appendRect(m_vertices, GhostNoteStyle::barRect(left, right, top, barHeight),
                   fillColors.fill(it->colorIndex), dpr);
    }
}
