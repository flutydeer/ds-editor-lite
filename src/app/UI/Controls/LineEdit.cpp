//
// Created by fluty on 24-2-20.
//

#include "LineEdit.h"

#include <QContextMenuEvent>
#include <QMWidgets/cmenu.h>

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
    if (const auto qMenu = createStandardContextMenu()) {
        const auto menu = new CMenu(this);
        for (const auto action : qMenu->actions()) {
            action->setParent(menu);
            menu->addAction(action);
        }
        delete qMenu;
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
    }
}