//
// Created by fluty on 24-2-20.
//

#include <QWheelEvent>
#include <QStyledItemDelegate>
#include <QAbstractItemView>

#include "ComboBox.h"
#include "Utils/SystemUtils.h"

ComboBox::ComboBox(QWidget *parent) : CComboBox(parent) {
    initUi();
}

ComboBox::ComboBox(const bool scrollWheelChangeSelection, QWidget *parent)
    : CComboBox(parent), m_scrollWheelChangeSelection(scrollWheelChangeSelection) {
    initUi();
}

void ComboBox::wheelEvent(QWheelEvent *event) {
    if (m_scrollWheelChangeSelection)
        CComboBox::wheelEvent(event);
    else
        event->ignore();
}

void ComboBox::initUi() {
    const auto styledItemDelegate = new QStyledItemDelegate();
    setItemDelegate(styledItemDelegate);

    const auto container = dynamic_cast<QWidget *>(view()->parent());
    container->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    container->setAttribute(Qt::WA_TranslucentBackground, true);
    container->setAttribute(Qt::WA_WindowPropagation);
    container->setAttribute(Qt::WA_X11NetWmWindowTypeCombo);

#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11()) {
        setProperty("dwmBorder", true);
    }
#endif
}