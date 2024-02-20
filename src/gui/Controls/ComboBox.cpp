//
// Created by fluty on 24-2-20.
//

#include <QWheelEvent>
#include <QStyledItemDelegate>
#include <QAbstractItemView>

#include "ComboBox.h"

ComboBox::ComboBox(QWidget *parent) : QComboBox(parent) {
    // setAttribute(Qt::WA_StyledBackground);
    initUi();
}
ComboBox::ComboBox(bool scrollWheelChangeSelection, QWidget *parent) : QComboBox(parent) {
    // setAttribute(Qt::WA_StyledBackground);
    m_scrollWheelChangeSelection = scrollWheelChangeSelection;
    initUi();
}
void ComboBox::wheelEvent(QWheelEvent *event) {
    if (m_scrollWheelChangeSelection)
        QComboBox::wheelEvent(event);
    else
        event->ignore();
}
void ComboBox::initUi() {
    auto comboBoxQss =
        "QComboBox QAbstractItemView { padding: 4px; background-color: #202122;"
        "border: 1px solid #606060; "
        "border-radius: 6px; color: #F0F0F0; selection-background-color: #FFFFFF; }"
        "QComboBox QAbstractItemView:item { padding: 4px; border-radius: 4px; border: none; }"
        "QComboBox QAbstractItemView::item:hover { background-color: #f6f6f6; }"
        "QComboBox QAbstractItemView::item:selected { background-color: #329BBAFF; }";
    auto styledItemDelegate = new QStyledItemDelegate();
    setItemDelegate(styledItemDelegate);
    setStyleSheet(comboBoxQss);

    auto container = dynamic_cast<QWidget *>(view()->parent());
    container->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint| Qt::NoDropShadowWindowHint);
    container->setAttribute(Qt::WA_TranslucentBackground, true);
    container->setAttribute(Qt::WA_WindowPropagation);
    container->setAttribute(Qt::WA_X11NetWmWindowTypeCombo);
}