#ifndef LYRIC_TAB_CONTROLS_SPLITTER_ITEM_H
#define LYRIC_TAB_CONTROLS_SPLITTER_ITEM_H

#include <QGraphicsItem>
#include <QGraphicsView>
#include <QPen>

namespace FillLyric
{
    class SplitterItem final : public QGraphicsItem {
    public:
        explicit SplitterItem(const qreal &x, const qreal &y, const qreal &w, QGraphicsView *view,
                              QGraphicsItem *parent = nullptr);
        ~SplitterItem() override;

        qreal width() const;
        void setWidth(const qreal &w);

        qreal height() const;

        qreal deltaY() const;

        void setMargin(const qreal &margin);
        qreal margin() const;

    protected:
        QRectF boundingRect() const override;
        QPainterPath shape() const override;

        void setQss();

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    private:
        qreal m_width = 0;

        qreal m_lineHeight = 1;

        QPen m_pen = QPen(QColor(120, 120, 120), 0.5);

        qreal m_margin = 5;

        QGraphicsView *m_view;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_CONTROLS_SPLITTER_ITEM_H
