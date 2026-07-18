#ifndef DRAWNOTEHANDLER_H
#define DRAWNOTEHANDLER_H

#include "PianoRollEditHandler.h"

class NoteView;

class DrawNoteHandler : public PianoRollEditHandler {
public:
    DrawNoteHandler();
    ~DrawNoteHandler() override;

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;

    void commit() override;
    void discard() override;

    [[nodiscard]] Qt::Orientations edgeAutoScrollAxes() const override;
    void continueDragAt(const QPoint &viewportPos) override;

    void prepareForDrawingNote(int tick, int keyIndex, int initialLength = -1);

private:
    void updateDrawingAt(const QPoint &viewportPos);

    NoteView *m_currentDrawingNote = nullptr;
    bool m_drawing = false;
};

#endif // DRAWNOTEHANDLER_H
