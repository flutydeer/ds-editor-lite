#ifndef DSEDITORLITE_PATHITEMDELEGATE_H
#define DSEDITORLITE_PATHITEMDELEGATE_H

#include <QStyledItemDelegate>

/// Draws a path row with a reorder grip in a gutter on its left.
///
/// The grips are painted rather than installed as child widgets: a row is an
/// editable text item, so an item widget would fight the inline editor, and it
/// would be dropped by the takeItem()/insertItem() dance the Move Up and Move
/// Down buttons use. Painting keeps the grip attached to whatever the model
/// does to the row.
class PathItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    /// Width of the grip gutter. The whole gutter, not just the glyph, is what
    /// starts a reorder, so the target stays finger-sized.
    static constexpr int kGutterWidth = 28;

    explicit PathItemDelegate(QObject *parent = nullptr);

    /// The grip area of \p itemRect, in the same coordinate system as the rect.
    [[nodiscard]] static QRect handleRect(const QRect &itemRect);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

#endif // DSEDITORLITE_PATHITEMDELEGATE_H
