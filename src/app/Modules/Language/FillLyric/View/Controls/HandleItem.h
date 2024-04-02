#ifndef HANDLEITEM_H
#define HANDLEITEM_H

#include <QGraphicsScene>
#include <QGraphicsRectItem>

namespace FillLyric {
    class HandleItem final : public QObject, public QGraphicsRectItem {
        Q_OBJECT
    public:
        explicit HandleItem(QGraphicsItem *parent = nullptr);
        ~HandleItem() override;

        [[nodiscard]] qreal width() const;
        void setWidth(const qreal &w);

        [[nodiscard]] qreal height() const;
        void setHeight(const qreal &h);

        void setMargin(const qreal &margin);
        [[nodiscard]] qreal margin() const;

    Q_SIGNALS:
        void selectAll() const;

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

        [[nodiscard]] QRectF boundingRect() const override;
        [[nodiscard]] QPainterPath shape() const override;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

    private:
        enum State {
            Normal = 0,
            Hovered = 1,
            Selected = 2,
        };

        QBrush m_backgroundBrush[3] = {QColor(155, 186, 255), QColor(169, 196, 255),
                                       QColor(169, 196, 255)};
        QPen m_borderPen[3] = {QPen(QColor(112, 156, 255), 2), QPen(QColor(112, 156, 255), 2),
                               QPen(QColor(Qt::white), 2)};

        qreal mW = 16;
        qreal mH = 0;

        qreal m_margin = 3;
    };

} // FillLyric

#endif // HANDLEITEM_H
