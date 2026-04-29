#include "SplitLineIndicator.h"

#include "NoteView.h"
#include "Global/AppGlobal.h"

#include <QPen>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SplitLineIndicator::SplitLineIndicator(QGraphicsItem *parent) : QGraphicsPathItem(parent) {
    setPen(QPen(QColor(255, 100, 100), 2));
    setZValue(10);
    hide();
}

void SplitLineIndicator::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
}

SplitLineIndicator::UpdateResult SplitLineIndicator::updateIndicator(NoteView *noteView,
                                                                      int splitPos) {
    if (!noteView) {
        hide();
        m_lastNoteView.clear();
        return Hidden;
    }

    const auto noteLocalStart = noteView->rStart();
    const auto noteLocalEnd = noteLocalStart + noteView->length();

    if (splitPos <= noteLocalStart || splitPos >= noteLocalEnd) {
        hide();
        m_lastNoteView.clear();
        return Hidden;
    }

    m_lastNoteView = noteView;
    m_splitPos = splitPos;
    rebuildPath();
    show();
    return Shown;
}

NoteView *SplitLineIndicator::lastNoteView() const {
    return m_lastNoteView;
}

int SplitLineIndicator::lastTick() const {
    return m_lastTick;
}

void SplitLineIndicator::setLastTick(int tick) {
    m_lastTick = tick;
}

void SplitLineIndicator::clearState() {
    hide();
    m_lastNoteView.clear();
    m_splitPos = 0;
    m_lastTick = 0;
}

void SplitLineIndicator::afterSetScale() {
    if (isVisible() && m_lastNoteView)
        rebuildPath();
}

void SplitLineIndicator::afterSetVisibleRect() {
}

void SplitLineIndicator::rebuildPath() {
    if (!m_lastNoteView)
        return;

    constexpr double extensionLength = 8.0;
    constexpr double forkLength = 6.0;
    constexpr double forkAngle = 45.0;

    const auto x = tickToSceneX(m_splitPos);

    const auto noteRect = m_lastNoteView->rect();
    const auto noteScenePos = m_lastNoteView->scenePos();

    const auto lineTop = noteScenePos.y() - extensionLength;
    const auto lineBottom = noteScenePos.y() + noteRect.height() + extensionLength;
    const auto forkAngleRad = forkAngle * M_PI / 180.0;
    const auto forkOffsetX = forkLength * std::sin(forkAngleRad);
    const auto forkOffsetY = forkLength * std::cos(forkAngleRad);

    QPainterPath path;
    path.moveTo(x, lineTop);
    path.lineTo(x, lineBottom);

    path.moveTo(x, lineTop);
    path.lineTo(x - forkOffsetX, lineTop - forkOffsetY);
    path.moveTo(x, lineTop);
    path.lineTo(x + forkOffsetX, lineTop - forkOffsetY);

    path.moveTo(x, lineBottom);
    path.lineTo(x - forkOffsetX, lineBottom + forkOffsetY);
    path.moveTo(x, lineBottom);
    path.lineTo(x + forkOffsetX, lineBottom + forkOffsetY);

    setPath(path);
}

double SplitLineIndicator::tickToSceneX(double tick) const {
    return tick * scaleX() * m_pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
}
