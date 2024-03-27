#ifndef SPLITTERGRAPHICSITEM_H
#define SPLITTERGRAPHICSITEM_H

#include <QGraphicsItem>
#include <QGraphicsScene>
namespace LyricWrap {
    class SplitterItem final : public QGraphicsItem {
    public:
        explicit SplitterItem(const qreal &x, const qreal &y, const qreal &w, const qreal &lh = 1,
                              QGraphicsItem *parent = nullptr);
        ~SplitterItem() override;

        void setPos(const qreal &x, const qreal &y);

        qreal width() const;
        void setWidth(const qreal &w);

        qreal height() const;

        qreal deltaY() const;
        void setLineHeight(const qreal &lh);

        QPen pen() const;
        void setPen(const QPen &pen);

        QRectF boundingRect() const override;

        void setMargin(qreal margin);
        qreal margin() const;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

    private:
        qreal mX;
        qreal mY;
        qreal mW;
        qreal mH;

        qreal m_lineHeight;

        QPen m_pen = QPen(Qt::gray, 1);

        qreal m_margin = 5;
    };
}

#endif // SPLITTERGRAPHICSITEM_H
