#include <lite/GUI/Controls/PathItemDelegate.h>

#include <lite/GUI/Utils/IconUtils.h>

#include <QIcon>
#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionViewItem>

namespace {
    constexpr int kGripSize = 16;
}

PathItemDelegate::PathItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {
}

QRect PathItemDelegate::handleRect(const QRect &itemRect) {
    return {itemRect.left(), itemRect.top(), kGutterWidth, itemRect.height()};
}

void PathItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {
    // The base style draws the text, the selection and the editor frame;
    // shifting its rect keeps the gutter free for the grip. initStyleOption()
    // does not touch rect, so the shift survives the base call.
    QStyleOptionViewItem shifted = option;
    shifted.rect = option.rect.adjusted(kGutterWidth, 0, 0, 0);
    QStyledItemDelegate::paint(painter, shifted, index);

    const QRect slot = handleRect(option.rect);
    const QRect target(slot.left() + (slot.width() - kGripSize) / 2,
                       slot.top() + (slot.height() - kGripSize) / 2, kGripSize, kGripSize);
    IconUtils::createTintedSvgIcon(
        QStringLiteral(":/svg/icons/re_order_dots_vertical_16_regular.svg"),
        QSize(kGripSize, kGripSize), IconUtils::defaultActionPalette())
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
