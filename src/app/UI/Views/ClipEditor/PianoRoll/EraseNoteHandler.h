#ifndef ERASENOTEHANDLER_H
#define ERASENOTEHANDLER_H

#include "PianoRollEditHandler.h"

#include <QList>
#include <QPoint>

class NoteView;

class EraseNoteHandler : public PianoRollEditHandler {
public:
    EraseNoteHandler();
    ~EraseNoteHandler() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void commit() override;
    void discard() override;

private:
    void eraseNoteUnderPos(const QPoint &pos);

    QList<int> m_notesToErase;
    bool m_erasing = false;
};

#endif // ERASENOTEHANDLER_H
