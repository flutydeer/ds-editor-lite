#include <lite/GUI/Controls/LineEdit.h>

#include <lite/GUI/Controls/Menu.h>

#include <QContextMenuEvent>

LineEdit::LineEdit(QWidget *parent) : QLineEdit(parent) {
}

LineEdit::LineEdit(const QString &text, QWidget *parent) : QLineEdit(text, parent) {
}

void LineEdit::mousePressEvent(QMouseEvent *event) {
    QLineEdit::mousePressEvent(event);
    event->ignore();
}

Menu *LineEdit::createContextMenu(QWidget *parent) {
    return Menu::fromLineEdit(this, parent);
}

void LineEdit::contextMenuEvent(QContextMenuEvent *event) {
    if (const auto menu = createContextMenu(this)) {
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
    }
    event->accept();
}
