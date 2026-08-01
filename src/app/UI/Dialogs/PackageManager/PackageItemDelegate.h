#ifndef PACKAGEITEMDELEGATE_H
#define PACKAGEITEMDELEGATE_H

#include <QStyledItemDelegate>

class PackageItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit PackageItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    // Text colors resolve from the current theme at paint time; the option
    // palette is the fallback when the semantic token is unavailable.
    QColor titleColor(const QStyleOptionViewItem &option, bool selected) const;
    QColor descColor(const QStyleOptionViewItem &option, bool selected) const;

    int m_titlePixelSize = 13;
    int m_descPixelSize = 12;
    double m_paddingLeft = 8;
    double m_paddingRight = 8;
    double m_paddingTop = 6;
    double m_paddingBottom = 6;
    double m_spacing = 4;
};


#endif //PACKAGEITEMDELEGATE_H
