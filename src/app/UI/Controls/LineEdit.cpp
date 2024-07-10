//
// Created by fluty on 24-2-20.
//

#include <QMenu>
#include <QContextMenuEvent>

#include "ToolTipFilter.h"
#include "LineEdit.h"

LineEdit::LineEdit(QWidget *parent) : QLineEdit(parent) {
    // installEventFilter(new ToolTipFilter(this));
}
LineEdit::LineEdit(const QString &text, QWidget *parent) : QLineEdit(text, parent) {
    // installEventFilter(new ToolTipFilter(this));
}
void LineEdit::mousePressEvent(QMouseEvent *event) {
    QLineEdit::mousePressEvent(event);
    event->ignore();
}
void LineEdit::contextMenuEvent(QContextMenuEvent *event) {
    // QLineEdit::contextMenuEvent(event);
    if (QMenu *menu = createStandardContextMenu()) {
        menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        menu->setAttribute(Qt::WA_TranslucentBackground, true);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
    }
}