//
// Created by fluty on 24-2-20.
//

#include <QWheelEvent>
#include <QStyledItemDelegate>
#include <QAbstractItemView>

#include "ComboBox.h"

ComboBox::ComboBox(QWidget *parent) : QComboBox(parent) {
    initUi();
}
ComboBox::ComboBox(bool scrollWheelChangeSelection, QWidget *parent)
    : QComboBox(parent), m_scrollWheelChangeSelection(scrollWheelChangeSelection) {
    initUi();
}
void ComboBox::wheelEvent(QWheelEvent *event) {
    if (m_scrollWheelChangeSelection)
        QComboBox::wheelEvent(event);
    else
        event->ignore();
}
void ComboBox::initUi() {
    auto styledItemDelegate = new QStyledItemDelegate();
    setItemDelegate(styledItemDelegate);

    auto container = dynamic_cast<QWidget *>(view()->parent());
    container->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    container->setAttribute(Qt::WA_TranslucentBackground, true);
    container->setAttribute(Qt::WA_WindowPropagation);
    container->setAttribute(Qt::WA_X11NetWmWindowTypeCombo);
}