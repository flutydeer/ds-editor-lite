//
// Created by fluty on 24-2-20.
//

#include <QMenu>
#include <QContextMenuEvent>

#include "LineEdit.h"
LineEdit::LineEdit(QWidget *parent) : QLineEdit(parent) {
}
LineEdit::LineEdit(const QString &text, QWidget *parent) : QLineEdit(text, parent) {
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