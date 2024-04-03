#include "SplitterItem.h"

#include <QPainter>

namespace FillLyric {
    SplitterItem::SplitterItem(const qreal &x, const qreal &y, const qreal &w,
                               QGraphicsItem *parent)
        : QGraphicsItem(parent) {
        this->setX(x);
        this->setY(y);
        this->setWidth(w);
    }

    SplitterItem::~SplitterItem() = default;

    QRectF SplitterItem::boundingRect() const {
        return {0, 0, width(), height()};
    }

    QPainterPath SplitterItem::shape() const {
        QPainterPath path;
        path.addRect({0, m_margin, width(), m_lineHeight});
        return path;
    }

    qreal SplitterItem::width() const {
        return mW;
    }

    void SplitterItem::setWidth(const qreal &w) {
        mW = w;
        update();
    }

    qreal SplitterItem::height() const {
        return m_lineHeight + m_margin * 2;
    }

    qreal SplitterItem::deltaY() const {
        return m_lineHeight + m_margin + 1;
    }

    void SplitterItem::setLineHeight(const qreal &lh) {
        m_lineHeight = lh + m_margin * 2;
        update();
    }

    QPen SplitterItem::pen() const {
        return m_pen;
    }

    void SplitterItem::setPen(const QPen &pen) {
        if (m_pen != pen) {
            m_pen = pen;
            update();
        }
    }

    void SplitterItem::setMargin(const qreal &margin) {
        if (m_margin != margin) {
            m_margin = margin;
            update();
        }
    }
    qreal SplitterItem::margin() const {
        return m_margin;
    }
    void SplitterItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
        painter->setPen(m_pen);
        for (int i = 0; i < m_lineHeight; i++) {
            painter->drawLine(QPointF(0, m_margin + i), QPointF(mW, m_margin + i));
        }
    }
}