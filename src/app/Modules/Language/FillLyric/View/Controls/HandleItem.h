#ifndef HANDLEITEM_H
#define HANDLEITEM_H

#include <QGraphicsScene>
#include <QGraphicsRectItem>

namespace FillLyric {
    class HandleItem final : public QGraphicsRectItem {
    public:
        explicit HandleItem(const qreal &x, const qreal &y, const qreal &w, const qreal &h,
                            QGraphicsRectItem *parent = nullptr);
        ~HandleItem() override;

        [[nodiscard]] qreal width() const;
        void setWidth(const qreal &w);

        [[nodiscard]] qreal height() const;
        void setHeight(const qreal &h);

        [[nodiscard]] qreal deltaY() const;
        void setLineHeight(const qreal &lh);

        [[nodiscard]] QPen pen() const;
        void setPen(const QPen &pen);

        void setMargin(qreal margin);
        [[nodiscard]] qreal margin() const;

    protected:
        [[nodiscard]] QRectF boundingRect() const override;
        [[nodiscard]] QPainterPath shape() const override;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

    private:
        qreal mW = 16;
        qreal mH;

        qreal m_lineHeight;

        QPen m_pen = QPen(QColor(112, 156, 255), 1);
        QBrush m_brush = QBrush(QColor(112, 156, 255));

        qreal m_margin = 3;
    };

} // FillLyric

#endif // HANDLEITEM_H
