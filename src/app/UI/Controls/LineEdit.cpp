//
// Created by fluty on 24-2-20.
//

#include "LineEdit.h"

#include "UI/Controls/Menu.h"
#include "UI/Utils/IconUtils.h"

#include <QContextMenuEvent>

#include <array>

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

LineEdit::LineEdit(QWidget *parent) : QLineEdit(parent) {
}

LineEdit::LineEdit(const QString &text, QWidget *parent) : QLineEdit(text, parent) {
}

void LineEdit::mousePressEvent(QMouseEvent *event) {
    QLineEdit::mousePressEvent(event);
    event->ignore();
}

Menu *LineEdit::createContextMenu(QWidget *parent) {
    const auto standardMenu = createStandardContextMenu();
    if (!standardMenu)
        return nullptr;

    auto *menu = new Menu(parent ? parent : this);
    qsizetype actionIndex = 0;
    for (const auto action : standardMenu->actions()) {
        action->setParent(menu);
        if (!action->isSeparator() && actionIndex < kStandardActionIcons.size()) {
            action->setIcon(IconUtils::menuIcon(
                QString::fromLatin1(kStandardActionIcons.at(actionIndex))));
            ++actionIndex;
        }
        menu->addAction(action);
    }
    delete standardMenu;
    return menu;
}

void LineEdit::contextMenuEvent(QContextMenuEvent *event) {
    if (const auto menu = createContextMenu(this)) {
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
    }
    event->accept();
}
