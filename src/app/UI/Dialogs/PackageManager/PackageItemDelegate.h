//
// Created by FlutyDeer on 2025/8/1.
//

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
    struct NormalPalette {
        QColor colorTitle = {182, 183, 186};
        QColor colorDesc = {182, 183, 186, 140};
    };

    struct SelectedPalette {
        QColor colorTitle = {155, 186, 255};
        QColor colorDesc = {155, 186, 255};
    };

    NormalPalette m_normalPalette;
    SelectedPalette m_selectedPalette;
    int m_titlePixelSize = 13;
    int m_descPixelSize = 12;
    double m_paddingLeft = 8;
    double m_paddingRight = 8;
    double m_paddingTop = 6;
    double m_paddingBottom = 6;
    double m_spacing = 4;
};


#endif //PACKAGEITEMDELEGATE_H