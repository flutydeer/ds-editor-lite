#include "PhonicTextEdit.h"

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

} // FillLyric