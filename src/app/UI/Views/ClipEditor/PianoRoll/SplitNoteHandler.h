#ifndef SPLITNOTEHANDLER_H
#define SPLITNOTEHANDLER_H

#include "PianoRollEditHandler.h"

class NoteView;
class SplitLineIndicator;

class SplitNoteHandler : public PianoRollEditHandler {
public:
    SplitNoteHandler();
    ~SplitNoteHandler() override;

    void activate() override;
    void deactivate() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;

private:
    void splitNoteAtPosition(NoteView *noteView, int tick);
    void updateIndicator(NoteView *noteView, int tick);

    SplitLineIndicator *m_indicator = nullptr;
    int m_lastTick = 0;
};

#endif // SPLITNOTEHANDLER_H
