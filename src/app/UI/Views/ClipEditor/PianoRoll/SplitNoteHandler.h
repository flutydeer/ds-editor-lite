#ifndef SPLITNOTEHANDLER_H
#define SPLITNOTEHANDLER_H

#include "PianoRollEditHandler.h"

class NoteView;
class QColor;
class SplitLineIndicator;

class SplitNoteHandler : public PianoRollEditHandler {
public:
    SplitNoteHandler();
    ~SplitNoteHandler() override;

    void activate() override;
    void deactivate() override;
    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;

    void applySplitLineColor(const QColor &color);

private:
    void updateIndicator(NoteView *noteView, int tick);

    SplitLineIndicator *m_indicator = nullptr;
    int m_lastTick = 0;
};

#endif // SPLITNOTEHANDLER_H
