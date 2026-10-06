#include <lite/GUI/Controls/PathItemDelegate.h>

#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Utils/IconUtils.h>

#include <QIcon>
#include <QModelIndex>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

namespace {
    constexpr int kGripSize = 16;

    /// The grip follows the row it belongs to the same way the options sidebar
    /// follows its item: accent while the row is selected, the plain icon color
    /// otherwise, mirroring the ::item color rules in the theme style sheets.
    QColor gripColor(const QStyleOptionViewItem &option) {
        auto palette = IconUtils::defaultActionPalette();
        if ((option.state & QStyle::State_Selected) != 0) {
            if (const auto accent =
                    ThemeManager::instance()->semanticColor(QStringLiteral("text.accent"));
                accent.isValid()) {
                palette.normal = accent;
            }
        }
        return palette.normal;
    }
}

PathItemDelegate::PathItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {
}

QRect PathItemDelegate::handleRect(const QRect &itemRect) {
    return {itemRect.left(), itemRect.top(), kGutterWidth, itemRect.height()};
}

void PathItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {
    // The base style draws the text, the selection and the editor frame. It gets
    // the whole row so the hover/selected fill covers the grip gutter as well,
    // and the text is held clear of the grip by the item's left padding instead
    // (see PathListWidget::item in the theme style sheets).
    QStyledItemDelegate::paint(painter, option, index);

    const QRect slot = handleRect(option.rect);
    const QRect target(slot.left() + (slot.width() - kGripSize) / 2,
                       slot.top() + (slot.height() - kGripSize) / 2, kGripSize, kGripSize);
    IconUtils::createTintedSvgIcon(
        QStringLiteral(":/svg/icons/re_order_dots_vertical_16_regular.svg"),
        QSize(kGripSize, kGripSize), gripColor(option))
        .paint(painter, target);
}

QSize PathItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const {
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    // Rows have to be tall enough to hold the grip even when the text is small.
    return {base.width() + kGutterWidth, qMax(base.height(), kGripSize + 8)};
}

void PathItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {
    QStyleOptionViewItem shifted = option;
    shifted.rect = option.rect.adjusted(kGutterWidth, 0, 0, 0);
    QStyledItemDelegate::updateEditorGeometry(editor, shifted, index);
}
