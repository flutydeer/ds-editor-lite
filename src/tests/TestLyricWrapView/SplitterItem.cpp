#include "SplitterItem.h"

#include <QPainter>

namespace LyricWrap {
    SplitterItem::SplitterItem(const qreal &x, const qreal &y, const qreal &w, const qreal &lh,
                               QGraphicsItem *parent)
        : mX(x), mY(y), mW(w), m_lineHeight(lh), QGraphicsItem(parent) {
        mH = m_lineHeight + m_margin * 2;
    }

    SplitterItem::~SplitterItem() = default;

    QRectF SplitterItem::boundingRect() const {
        return {mX, mY, mW, mH};
    }

    void SplitterItem::setPos(const qreal &x, const qreal &y) {
        mX = x;
        mY = y;
        update();
    }

    qreal SplitterItem::width() const {
        return mY;
    }

    void SplitterItem::setWidth(const qreal &w) {
        mW = w;
        update();
    }

    qreal SplitterItem::height() const {
        return mH + m_margin * 2;
    }

    qreal SplitterItem::deltaY() const{
        return m_lineHeight / 2 + m_margin;
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

    void SplitterItem::setMargin(qreal margin) {
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
            painter->drawLine(QPointF(mX + m_margin, mY + m_margin + i),
                              QPointF(mX + mW - m_margin, mY + m_margin + i));
        }
    }
}