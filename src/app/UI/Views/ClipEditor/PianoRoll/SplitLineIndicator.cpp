#include "SplitLineIndicator.h"

#include "NoteView.h"

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

SplitLineIndicator::UpdateResult SplitLineIndicator::updateIndicator(NoteView *noteView,
                                                                      int splitPos,
                                                                      double sceneX) {
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
    buildPath(sceneX, noteView);
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
    m_lastTick = 0;
}

void SplitLineIndicator::buildPath(double x, NoteView *noteView) {
    constexpr double extensionLength = 8.0;
    constexpr double forkLength = 6.0;
    constexpr double forkAngle = 45.0;

    const auto noteRect = noteView->rect();
    const auto noteScenePos = noteView->scenePos();

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
