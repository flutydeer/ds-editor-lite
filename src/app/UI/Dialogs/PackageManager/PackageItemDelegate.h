//
// Created by FlutyDeer on 2025/8/1.
//

#ifndef PACKAGEITEMDELEGATE_H
#define PACKAGEITEMDELEGATE_H

#include <QStyledItemDelegate>

class PackageItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
    Q_PROPERTY(QColor colorTitle READ colorTitle WRITE setColorTitle)
    Q_PROPERTY(QColor colorDesc READ colorDesc WRITE setColorDesc)
    Q_PROPERTY(int titlePixelSize READ titlePixelSize WRITE setTitlePixelSize)
    Q_PROPERTY(int descPixelSize READ descPixelSize WRITE setDescPixelSize)
    Q_PROPERTY(double paddingLeft READ paddingLeft WRITE setPaddingLeft)
    Q_PROPERTY(double paddingRight READ paddingRight WRITE setPaddingRight)
    Q_PROPERTY(double paddingTop READ paddingTop WRITE setPaddingTop)
    Q_PROPERTY(double paddingBottom READ paddingBottom WRITE setPaddingBottom)
    Q_PROPERTY(double spacing READ spacing WRITE setSpacing)

public:
    explicit PackageItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QColor colorTitle() const;
    void setColorTitle(const QColor &color);
    QColor colorDesc() const;
    void setColorDesc(const QColor &color);
    int titlePixelSize() const;
    void setTitlePixelSize(int pixelSize);
    int descPixelSize() const;
    void setDescPixelSize(int pixelSize);
    double paddingLeft() const;
    void setPaddingLeft(double padding);
    double paddingRight() const;
    void setPaddingRight(double padding);
    double paddingTop() const;
    void setPaddingTop(double padding);
    double paddingBottom() const;
    void setPaddingBottom(double padding);
    double spacing() const;
    void setSpacing(double spacing);

    QColor m_colorTitle = {215, 216, 219};
    QColor m_colorDesc = {215, 216, 219, 140};
    int m_titlePixelSize = 13;
    int m_descPixelSize = 12;
    double m_paddingLeft = 8;
    double m_paddingRight = 8;
    double m_paddingTop = 6;
    double m_paddingBottom = 7;
    double m_spacing = 4;

    std::string locale;
};


#endif //PACKAGEITEMDELEGATE_H