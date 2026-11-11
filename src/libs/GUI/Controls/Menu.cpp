#include <lite/GUI/Controls/Menu.h>

#include <lite/GUI/Utils/IconUtils.h>
#include <lite/Support/SystemUtils.h>

#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionMenuItem>
#include <QLineEdit>
#include <QEvent>
#include <QKeySequence>
#include <QStyle>

#include <array>
#include <utility>

Menu::Menu(QWidget *parent) : CMenu(parent) {
    initUi();
}

Menu::Menu(const QString &title, QWidget *parent) : CMenu(title, parent) {
    initUi();
}

Menu *Menu::fromLineEdit(QLineEdit *editor, QWidget *parent) {
    if (!editor)
        return nullptr;
    auto *standardMenu = editor->createStandardContextMenu();
    if (!standardMenu)
        return nullptr;

    auto *menu = new Menu(parent ? parent : editor);
    // Retain Qt's action and submenu ownership while presenting the styled menu.
    standardMenu->setParent(menu);
    constexpr std::array<std::pair<const char *, const char *>, 7> icons = {
        {
         {"edit-undo", ":/svg/icons/arrow_undo_16_regular.svg"},
         {"edit-redo", ":/svg/icons/arrow_redo_16_regular.svg"},
         {"edit-cut", ":/svg/icons/cut_16_regular.svg"},
         {"edit-copy", ":/svg/icons/copy_16_regular.svg"},
         {"edit-paste", ":/svg/icons/clipboard_paste_16_regular.svg"},
         {"edit-delete", ":/svg/icons/delete_16_regular.svg"},
         {"select-all", ":/svg/icons/select_all_on_16_regular.svg"},
         }
    };
    for (auto *action : standardMenu->actions()) {
        for (const auto &[name, path] : icons) {
            if (action->objectName() == QLatin1StringView(name)) {
                action->setIcon(IconUtils::menuIcon(QString::fromLatin1(path)));
                break;
            }
        }
        menu->addAction(action);
    }
    return menu;
}

QColor Menu::shortcutTextColor() const {
    return m_shortcutTextColor;
}

void Menu::setShortcutTextColor(const QColor &color) {
    m_shortcutTextColor = color;
}

QColor Menu::shortcutSelectedTextColor() const {
    return m_shortcutSelectedTextColor;
}

void Menu::setShortcutSelectedTextColor(const QColor &color) {
    m_shortcutSelectedTextColor = color;
}

void Menu::initUi() {
#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11()) {
        setProperty("dwmBorder", true);
    }
#endif
}

void Menu::updateSubMenuArrowSpace() {
    bool hasSubMenu = false;
    bool hasShortcutColumn = false;
    for (const auto action : actions()) {
        if (!action)
            continue;
        if (action->menu())
            hasSubMenu = true;
        if (!action->shortcut().isEmpty() || action->text().contains(QLatin1Char('\t')))
            hasShortcutColumn = true;
    }

    // QStyleSheetStyle::sizeFromContents(CT_MenuItem) never reserves room for the
    // ::right-arrow subcontrol (QWindowsStyle adds windowsArrowHMargin for SubMenu
    // items). Extra padding-right supplies that room, but it also moves
    // QRenderRule::contentsRect().right(), which is exactly where CE_MenuItem
    // right-aligns shortcut text. A menu that already has a shortcut column would
    // therefore shift its whole shortcut column to the left. Those menus keep the
    // default padding: the shortcut column owns the right edge and the arrow
    // shares that column on different rows.
    const bool needsSpace = hasSubMenu && !hasShortcutColumn;

    const QVariant current = property("submenuArrowSpace");
    if (current.isValid() && current.toBool() == needsSpace)
        return;

    // Re-polish because attribute selectors are only re-evaluated on polish.
    setProperty("submenuArrowSpace", needsSpace);
    if (style()) {
        style()->unpolish(this);
        style()->polish(this);
    }
}

bool Menu::event(QEvent *event) {
    switch (event->type()) {
        case QEvent::ActionAdded:
        case QEvent::ActionChanged:
        case QEvent::ActionRemoved:
            // Run before CMenu::event(), which re-polishes for its `stats` property.
            updateSubMenuArrowSpace();
            break;
        default:
            break;
    }
    return CMenu::event(event);
}

void Menu::paintEvent(QPaintEvent *event) {
    CMenu::paintEvent(event);

    QPainter painter(this);
    const QSize indicatorSize(16, 16);
    const QSize arrowSize(14, 14);
    const qreal dpr = devicePixelRatioF();
    // Qt paints the label and the shortcut text with the same pen, and Qt style
    // sheets have no ::shortcut subcontrol. When the theme supplies its own
    // shortcut colours, repaint that text ourselves after the base pass.
    const bool customShortcutColor = m_shortcutTextColor.isValid();
    // Keep in sync with QMenu::item in controls.qss. QStyleSheetStyle right-aligns
    // shortcut text to contentsRect().right(), i.e. item.right() - padding-right.
    const int paddingRight = property("submenuArrowSpace").toBool() ? 36 : 6;
    // The custom chevron covers item.right()-20 .. -7. QStyleSheetStyle's
    // sizeFromContents() reserves no arrow width for a tabbed row (QWindowsStyle
    // adds tabSpacing instead), so a row carrying BOTH a chevron and a shortcut
    // must move its shortcut left by (20px arrow inset + 12px gap + 2px slack
    // - its own right inset) to keep them apart.
    const int chevronShift = qMax(0, 34 - paddingRight);

    for (const auto action : actions()) {
        if (!action || action->isSeparator() || !event->rect().intersects(actionGeometry(action)))
            continue;

        QStyleOptionMenuItem option;
        initStyleOption(&option, action);
        option.rect = actionGeometry(action);

        const QColor color = option.palette.color(
            (option.state & QStyle::State_Enabled) ? QPalette::Active : QPalette::Disabled,
            QPalette::Text);

        const int tab = option.text.indexOf(QLatin1Char('\t'));
        // Disabled rows stay on Qt's native pass. CE_MenuItem draws the label
        // and the shortcut with one pen, and the themed disabled colour comes
        // from QMenu::item:disabled inside QStyleSheetStyle -- it is not exposed
        // through QStyleOptionMenuItem::palette (neither QPalette::Text nor
        // buttonText() returns it). Repainting here would put the shortcut back
        // in an enabled-looking colour. With repaintShortcut false, clearSlot()
        // also keeps the full text, so Qt redraws the shortcut itself.
        const bool rowEnabled = (option.state & QStyle::State_Enabled) != 0;
        const bool repaintShortcut = customShortcutColor && rowEnabled && tab >= 0;
        const bool hasSubMenu = action->menu() != nullptr;
        const QString shortcutText = repaintShortcut ? option.text.mid(tab + 1) : QString();

        auto clearSlot = [&](const QRect &slot) {
            QStyleOption panelOption;
            panelOption.initFrom(this);
            panelOption.rect = rect();

            auto cleanOption = option;
            cleanOption.menuItemType = QStyleOptionMenuItem::Normal;
            cleanOption.checkType = QStyleOptionMenuItem::NotCheckable;
            cleanOption.checked = false;
            cleanOption.icon = {};
            if (repaintShortcut) {
                // Strip the shortcut only when the pass below will actually
                // redraw it. Otherwise a submenu row with a shortcut would have
                // its text erased by clearSlot(arrowSlot) and never restored.
                const int cleanTab = cleanOption.text.indexOf(QLatin1Char('\t'));
                if (cleanTab >= 0)
                    cleanOption.text.truncate(cleanTab);
            }

            painter.save();
            painter.setClipRect(slot);
            style()->drawPrimitive(QStyle::PE_PanelMenu, &panelOption, &painter, this);
            style()->drawControl(QStyle::CE_MenuItem, &cleanOption, &painter, this);
            painter.restore();
        };

        const QRect indicatorSlot(option.rect.left(), option.rect.top(), 32,
                                  option.rect.height());
        const QRect arrowSlot(option.rect.right() - 36, option.rect.top(), 37,
                              option.rect.height());

        int textLeft = 0;
        int textWidth = 0;
        QRect eraseRect;
        if (repaintShortcut) {
            textWidth = option.fontMetrics.horizontalAdvance(shortcutText);
            textLeft = option.rect.right() - paddingRight - textWidth + 1 -
                       (hasSubMenu ? chevronShift : 0);
            // Cover both the original shortcut position and the shifted one.
            eraseRect = QRect(textLeft - 2, option.rect.top(),
                              option.rect.right() - paddingRight + 2 - (textLeft - 2),
                              option.rect.height());
        }

        // Erase first: every clearSlot() redraws the row without its shortcut,
        // so anything drawn before the last erase would be wiped out. Custom
        // elements are drawn afterwards, with the chevron last so it stays on
        // top of the erased shortcut area.
        if (option.checked)
            clearSlot(indicatorSlot);
        if (hasSubMenu)
            clearSlot(arrowSlot);
        if (repaintShortcut)
            clearSlot(eraseRect);

        if (option.checked) {
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

        if (repaintShortcut) {
            QColor shortcutColor = m_shortcutTextColor;
            // Disabled rows never reach this pass; see repaintShortcut above.
            if ((option.state & QStyle::State_Selected) &&
                m_shortcutSelectedTextColor.isValid())
                shortcutColor = m_shortcutSelectedTextColor;

            int textFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic |
                            Qt::TextDontClip | Qt::TextSingleLine;
            if (!style()->styleHint(QStyle::SH_UnderlineShortcut, &option, this))
                textFlags |= Qt::TextHideMnemonic;

            const QRect textRect(textLeft, option.rect.top(), textWidth + 2,
                                 option.rect.height());
            painter.save();
            painter.setFont(option.font);
            painter.setPen(shortcutColor);
            painter.drawText(textRect, textFlags, shortcutText);
            painter.restore();
        }

        if (hasSubMenu) {
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
