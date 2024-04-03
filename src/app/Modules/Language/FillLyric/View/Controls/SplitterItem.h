#ifndef SPLITTERGRAPHICSITEM_H
#define SPLITTERGRAPHICSITEM_H

#include <QGraphicsItem>
#include <QGraphicsScene>

namespace FillLyric {
    class SplitterItem final : public QGraphicsItem {
    public:
        explicit SplitterItem(const qreal &x, const qreal &y, const qreal &w,
                              QGraphicsItem *parent = nullptr);
        ~SplitterItem() override;

        [[nodiscard]] qreal width() const;
        void setWidth(const qreal &w);

        [[nodiscard]] qreal height() const;

        [[nodiscard]] qreal deltaY() const;
        void setLineHeight(const qreal &lh);

        [[nodiscard]] QPen pen() const;
        void setPen(const QPen &pen);

        void setMargin(const qreal &margin);
        [[nodiscard]] qreal margin() const;

    protected:
        [[nodiscard]] QRectF boundingRect() const override;
        [[nodiscard]] QPainterPath shape() const override;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

    private:
        qreal mW = 0;

        qreal m_lineHeight = 1;

        QPen m_pen = QPen(QColor(120, 120, 120), 1);

        qreal m_margin = 5;
    };
}

#endif // SPLITTERGRAPHICSITEM_H
