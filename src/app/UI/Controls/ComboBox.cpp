//
// Created by fluty on 24-2-20.
//

#include <QWheelEvent>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QPainter>
#include <QStyleOptionComboBox>

#include <array>

#include "ComboBox.h"
#include "UI/Controls/Menu.h"
#include "UI/Utils/IconUtils.h"
#include <lite/Support/SystemUtils.h>

namespace {
    constexpr std::array<const char *, 7> kStandardActionIcons = {
        ":/svg/icons/arrow_undo_16_regular.svg",
        ":/svg/icons/arrow_redo_16_regular.svg",
        ":/svg/icons/cut_16_regular.svg",
        ":/svg/icons/copy_16_regular.svg",
        ":/svg/icons/clipboard_paste_16_regular.svg",
        ":/svg/icons/delete_16_regular.svg",
        ":/svg/icons/select_all_on_16_regular.svg",
    };
}

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

    QRect arrowRect =
        style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, this);
    if (arrowRect.isEmpty())
        arrowRect = QRect(width() - 28, 0, 28, height());

    const QSize iconSize(16, 16);
    const QPoint iconPos(arrowRect.x() + (arrowRect.width() - iconSize.width()) / 2,
                         arrowRect.y() + (arrowRect.height() - iconSize.height()) / 2);
    const QColor iconColor = option.palette.color(
        isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);
    const auto pixmap =
        IconUtils::renderTintedSvgPixmap(QStringLiteral(":/svg/icons/chevron_down_16_regular.svg"),
                                         iconSize, iconColor, devicePixelRatioF());
    painter.drawPixmap(iconPos, pixmap);
}

void ComboBox::wheelEvent(QWheelEvent *event) {
    if (m_scrollWheelChangeSelection)
        CComboBox::wheelEvent(event);
    else
        event->ignore();
}

Menu *ComboBox::createContextMenu(QWidget *parent) {
    if (!lineEdit())
        return nullptr;

    const auto standardMenu = lineEdit()->createStandardContextMenu();
    if (!standardMenu)
        return nullptr;

    auto *menu = new Menu(parent ? parent : this);
    qsizetype actionIndex = 0;
    for (const auto action : standardMenu->actions()) {
        action->setParent(menu);
        if (!action->isSeparator() && actionIndex < kStandardActionIcons.size()) {
            action->setIcon(
                IconUtils::menuIcon(QString::fromLatin1(kStandardActionIcons.at(actionIndex))));
            ++actionIndex;
        }
        menu->addAction(action);
    }
    delete standardMenu;
    return menu;
}

void ComboBox::contextMenuEvent(QContextMenuEvent *event) {
    // The embedded line edit of an editable combo box has Qt::NoContextMenu policy,
    // so its context menu events always arrive here; show the app-styled menu instead
    // of letting QComboBox forward the event to the native line edit menu
    if (const auto menu = createContextMenu(this)) {
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
        event->accept();
        return;
    }
    CComboBox::contextMenuEvent(event);
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
