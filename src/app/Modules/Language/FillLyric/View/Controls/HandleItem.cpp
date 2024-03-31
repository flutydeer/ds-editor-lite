#include "HandleItem.h"

#include <QPainter>
#include <QGraphicsScene>

namespace FillLyric {
    HandleItem::HandleItem(const qreal &x, const qreal &y, const qreal &w, const qreal &h,
                           QGraphicsRectItem *parent)
        : QGraphicsRectItem(parent), mW(w), mH(h) {
        this->setPos(x, y);
        this->setPen(QPen(Qt::gray, 1));
    }

    HandleItem::~HandleItem() = default;

    QRectF HandleItem::boundingRect() const {
        return {0, 0, width(), height()};
    }

    QPainterPath HandleItem::shape() const {
        QPainterPath path;
        path.addRect({0, 0, 0, 0});
        return path;
    }

    void HandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                           QWidget *widget) {
        const auto boxRect =
            QRectF(m_margin, m_margin, width() - m_margin * 2, height() - m_margin * 2);

        painter->setPen(m_pen);
        painter->setBrush(m_brush);
        painter->drawRoundedRect(boxRect, m_margin * 0.5, m_margin * 0.5);
    }

    void HandleItem::setWidth(const qreal &w) {
        mW = w;
        this->setRect({x(), y(), mW, mH});
    }

    qreal HandleItem::width() const {
        return mW;
    }

    void HandleItem::setHeight(const qreal &h) {
        mH = h;
        this->setRect({x(), y(), mW, mH});
    }

    qreal HandleItem::height() const {
        return mH;
    }

    QPen HandleItem::pen() const {
        return m_pen;
    }

    void HandleItem::setPen(const QPen &pen) {
        m_pen = pen;
    }

    void HandleItem::setMargin(qreal margin) {
        m_margin = margin;
    }

    qreal HandleItem::margin() const {
        return m_margin;
    }


} // FillLyric