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

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;

    void commit() override;
    void discard() override;

    [[nodiscard]] Qt::Orientations edgeAutoScrollAxes() const override;
    void continueDragAt(const QPoint &viewportPos) override;

private:
    void eraseNoteUnderPos(const QPoint &pos);

    QList<int> m_notesToErase;
    bool m_erasing = false;
};

#endif // ERASENOTEHANDLER_H
