#include "PhonicTextEdit.h"

#include <QMenu>

namespace FillLyric {

    PhonicTextEdit::PhonicTextEdit(QWidget *parent) : QPlainTextEdit(parent) {
        QFont f = this->font();
        f.setPointSizeF(11);
        setFont(f);
    }

    void PhonicTextEdit::wheelEvent(QWheelEvent *event) {
        if (event->modifiers() & Qt::ControlModifier) {
            const auto fontSizeDelta = event->angleDelta().y() / 120.0;
            QFont font = this->font();
            const auto newSize = font.pointSizeF() + fontSizeDelta;
            if (newSize > 0) {
                font.setPointSizeF(newSize);
                this->setFont(font);
                Q_EMIT fontChanged();
            }
            event->accept();
            return;
        }
        QPlainTextEdit::wheelEvent(event);
    }

    void PhonicTextEdit::contextMenuEvent(QContextMenuEvent *event) {
        QMenu *menu = createStandardContextMenu();

        menu->setAttribute(Qt::WA_TranslucentBackground);
        menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint |
                             Qt::NoDropShadowWindowHint);

        menu->exec(event->globalPos());
        delete menu;
    }

} // FillLyric