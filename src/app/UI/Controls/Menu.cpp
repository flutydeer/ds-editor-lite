//
// Created by FlutyDeer on 2026/4/25.
//

#include "Menu.h"

#include "UI/Utils/IconUtils.h"
#include <lite/Support/SystemUtils.h>

#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionMenuItem>

Menu::Menu(QWidget *parent) : CMenu(parent) {
    initUi();
}

Menu::Menu(const QString &title, QWidget *parent) : CMenu(title, parent) {
    initUi();
}

void Menu::initUi() {
#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11()) {
        setProperty("dwmBorder", true);
    }
#endif
}

void Menu::paintEvent(QPaintEvent *event) {
    CMenu::paintEvent(event);

    QPainter painter(this);
    const QSize indicatorSize(16, 16);
    const QSize arrowSize(14, 14);
    const qreal dpr = devicePixelRatioF();

    for (const auto action : actions()) {
        if (!action || action->isSeparator() || !event->rect().intersects(actionGeometry(action)))
            continue;

        QStyleOptionMenuItem option;
        initStyleOption(&option, action);
        option.rect = actionGeometry(action);

        const QColor color = option.palette.color(
            (option.state & QStyle::State_Enabled) ? QPalette::Active : QPalette::Disabled,
            QPalette::Text);

        auto clearSlot = [&](const QRect &slot) {
            QStyleOption panelOption;
            panelOption.initFrom(this);
            panelOption.rect = rect();

            auto cleanOption = option;
            cleanOption.menuItemType = QStyleOptionMenuItem::Normal;
            cleanOption.checkType = QStyleOptionMenuItem::NotCheckable;
            cleanOption.checked = false;
            cleanOption.icon = {};

            painter.save();
            painter.setClipRect(slot);
            style()->drawPrimitive(QStyle::PE_PanelMenu, &panelOption, &painter, this);
            style()->drawControl(QStyle::CE_MenuItem, &cleanOption, &painter, this);
            painter.restore();
        };

        if (option.checked) {
            const QRect indicatorSlot(option.rect.left(), option.rect.top(), 32,
                                      option.rect.height());
            clearSlot(indicatorSlot);

            const QRect indicatorRect(indicatorSlot.left() + 6,
                                      indicatorSlot.top() +
                                          (indicatorSlot.height() - indicatorSize.height()) / 2,
                                      indicatorSize.width(), indicatorSize.height());
            const QPoint iconPos(
                indicatorRect.x() + (indicatorRect.width() - indicatorSize.width()) / 2,
                indicatorRect.y() + (indicatorRect.height() - indicatorSize.height()) / 2);
            const auto pixmap = IconUtils::renderTintedSvgPixmap(
                QStringLiteral(":/svg/icons/checkmark_16_filled.svg"), indicatorSize, color, dpr);
            painter.drawPixmap(iconPos, pixmap);
        }

        if (action->menu()) {
            const QRect arrowSlot(option.rect.right() - 36, option.rect.top(), 37,
                                  option.rect.height());
            clearSlot(arrowSlot);

            const QRect arrowRect(option.rect.right() - 6 - arrowSize.width(),
                                  option.rect.top() +
                                      (option.rect.height() - arrowSize.height()) / 2,
                                  arrowSize.width(), arrowSize.height());
            const auto pixmap = IconUtils::renderTintedSvgPixmap(
                QStringLiteral(":/svg/icons/chevron_right_16_regular.svg"), arrowSize, color, dpr);
            painter.drawPixmap(arrowRect.topLeft(), pixmap);
        }
    }
}
