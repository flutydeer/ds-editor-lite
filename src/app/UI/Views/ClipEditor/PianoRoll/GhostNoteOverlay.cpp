#include "GhostNoteOverlay.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QPainter>

#include <algorithm>

using namespace ClipEditorGlobal;

GhostNoteOverlay::GhostNoteOverlay() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

void GhostNoteOverlay::setSource(const GhostNoteSource *source) {
    m_source = source;
    update();
}

void GhostNoteOverlay::setOffset(const int offset) {
    if (m_offset == offset)
        return;
    m_offset = offset;
    update();
}

void GhostNoteOverlay::updateRectAndPos() {
    const auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

void GhostNoteOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)
    if (!m_source || !m_source->enabled())
        return;
    const auto &ghosts = m_source->notes();
    if (ghosts.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(Qt::NoPen);

    const auto rowHeight = noteHeight * scaleY();
    const auto barHeight = std::max(GhostNoteStyle::minHeight,
                                    rowHeight * GhostNoteStyle::heightRatio);
    const auto visibleStart = startTick() + m_offset;
    const auto visibleEnd = endTick() + m_offset;
    // 音符有长度，起点可能落在可见区间左侧，故从 visibleStart - maxLength 开始扫
    const auto scanFrom = visibleStart - m_source->maxLength();
    const auto first =
        std::lower_bound(ghosts.begin(), ghosts.end(), scanFrom,
                         [](const GhostNote &note, const double tick) {
                             return note.globalStart < tick;
                         });

    for (auto it = first; it != ghosts.end(); ++it) {
        if (it->globalStart > visibleEnd)
            break;
        if (it->globalStart + it->length < visibleStart)
            continue;
        const auto top = sceneYToItemY((127 - it->keyIndex) * rowHeight) +
                         (rowHeight - barHeight) * 0.5;
        if (top + barHeight < 0 || top > visibleRect().height())
            continue;
        const auto left = tickToItemX(it->globalStart - m_offset);
        const auto right = tickToItemX(it->globalStart + it->length - m_offset);
        painter->fillRect(QRectF(left, top, std::max(1.0, right - left), barHeight),
                          GhostNoteStyle::fillColor(it->colorIndex));
    }
}
