//
// Created by fluty on 24-2-20.
//

#include <QWheelEvent>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <QPainter>
#include <QStyleOptionComboBox>

#include "ComboBox.h"
#include "UI/Utils/IconUtils.h"
#include "Utils/SystemUtils.h"

ComboBox::ComboBox(QWidget *parent) : CComboBox(parent) {
    initUi();
}

ComboBox::ComboBox(const bool scrollWheelChangeSelection, QWidget *parent)
    : CComboBox(parent), m_scrollWheelChangeSelection(scrollWheelChangeSelection) {
    initUi();
}

void ComboBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QStyleOptionComboBox option;
    initStyleOption(&option);

    QPainter painter(this);

    auto frameOption = option;
    frameOption.subControls &= ~QStyle::SC_ComboBoxArrow;
    style()->drawComplexControl(QStyle::CC_ComboBox, &frameOption, &painter, this);
    style()->drawControl(QStyle::CE_ComboBoxLabel, &option, &painter, this);

    QRect arrowRect = style()->subControlRect(QStyle::CC_ComboBox, &option,
                                              QStyle::SC_ComboBoxArrow, this);
    if (arrowRect.isEmpty())
        arrowRect = QRect(width() - 28, 0, 28, height());

    const QSize iconSize(16, 16);
    const QPoint iconPos(arrowRect.x() + (arrowRect.width() - iconSize.width()) / 2,
                         arrowRect.y() + (arrowRect.height() - iconSize.height()) / 2);
    const QColor iconColor = option.palette.color(isEnabled() ? QPalette::Active
                                                              : QPalette::Disabled,
                                                  QPalette::ButtonText);
    const auto pixmap = IconUtils::renderTintedSvgPixmap(
        QStringLiteral(":/svg/icons/chevron_down_16_filled.svg"), iconSize, iconColor,
        devicePixelRatioF());
    painter.drawPixmap(iconPos, pixmap);
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
