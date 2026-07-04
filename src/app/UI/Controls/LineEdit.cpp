//
// Created by fluty on 24-2-20.
//

#include "LineEdit.h"

#include <QContextMenuEvent>
#include "UI/Controls/Menu.h"

LineEdit::LineEdit(QWidget *parent) : QLineEdit(parent) {
}

LineEdit::LineEdit(const QString &text, QWidget *parent) : QLineEdit(text, parent) {
}

void LineEdit::mousePressEvent(QMouseEvent *event) {
    QLineEdit::mousePressEvent(event);
    event->ignore();
}

void LineEdit::contextMenuEvent(QContextMenuEvent *event) {
    if (const auto qMenu = createStandardContextMenu()) {
        const auto menu = new Menu(this);
        for (const auto action : qMenu->actions()) {
            action->setParent(menu);
            menu->addAction(action);
        }
        delete qMenu;
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
    }
}