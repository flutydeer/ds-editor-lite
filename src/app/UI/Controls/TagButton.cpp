#include "TagButton.h"

TagButton::TagButton(QWidget *parent) : QPushButton(parent) {
    setCheckable(true);
    setAttribute(Qt::WA_StyledBackground);
}

TagButton::TagButton(const QString &text, QWidget *parent) : QPushButton(text, parent) {
    setCheckable(true);
    setAttribute(Qt::WA_StyledBackground);
}
