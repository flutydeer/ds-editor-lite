//
// Created by fluty on 2023/8/13.
//
#include "EditLabel.h"

#include <QHBoxLayout>
#include <QLabel>

#include "EditDialog.h"

EditLabel::EditLabel(QWidget *parent) : QStackedWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_Hover, true);

    label = new QLabel;
    label->setText("Label");
    label->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    label->installEventFilter(this);
    label->setStyleSheet(QString("padding: 2px;"));

    this->addWidget(label);
    this->setCurrentWidget(label);
}

void EditLabel::mouseDoubleClickEvent(QMouseEvent *event) {
    const auto editRect = QRectF(label->x(), label->y(), label->width(), label->height());
    EditDialog dlg(label->text(), editRect, this->font(), this);
    dlg.setModal(true);
    dlg.exec();
    if (dlg.text != label->text()) {
        label->setText(dlg.text);
        Q_EMIT this->editCompleted(dlg.text);
    }
}

QString EditLabel::text() const {
    return label->text();
}

void EditLabel::setText(const QString &text) {
    setCurrentWidget(label);
    label->setText(text);
}