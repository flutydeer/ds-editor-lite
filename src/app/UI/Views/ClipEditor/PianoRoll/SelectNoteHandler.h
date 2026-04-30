#ifndef SELECTNOTEHANDLER_H
#define SELECTNOTEHANDLER_H

#include "PianoRollEditHandler.h"

class SelectNoteHandler : public PianoRollEditHandler {
public:
    SelectNoteHandler();
    ~SelectNoteHandler() override;

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;
};

#endif // SELECTNOTEHANDLER_H
