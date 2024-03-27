#include "LyricLineEdit.h"

namespace LyricWrap {
    LyricLineEdit::LyricLineEdit(QWidget *parent) : QLineEdit(parent) {
        setStyleSheet("QLineEdit { border: none; }");
        setMinimumWidth(30);
        connect(this, &QLineEdit::textChanged, this, &LyricLineEdit::adjustMaxWidth);
    }

    void LyricLineEdit::adjustMaxWidth() {
        const int textWidth = fontMetrics().horizontalAdvance(this->text()) + 10;
        setFixedWidth(textWidth);
    }
} // LyricWarp