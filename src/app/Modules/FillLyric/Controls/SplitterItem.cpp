#include "Modules/FillLyric/Controls/SplitterItem.h"

#include <QPainter>

#include "Modules/FillLyric/Utils/QssParser.h"

namespace FillLyric
{
    SplitterItem::SplitterItem(const qreal &x, const qreal &y, const qreal &w, QGraphicsView *view,
                               QGraphicsItem *parent) : QGraphicsItem(parent), m_view(view) {
        this->setX(x);
        this->setY(y);
        this->setWidth(w);
        this->setQss();
    }

    SplitterItem::~SplitterItem() = default;

    QRectF SplitterItem::boundingRect() const {
        if (this->isVisible())
            return {0, 0, width(), height()};
        return {0, 0, 0, m_margin / 2};
    }

    QPainterPath SplitterItem::shape() const {
        QPainterPath path;
        if (this->isVisible())
            path.addRect({0, m_margin, width(), m_lineHeight});
        else
            path.addRect({0, 0, 0, m_margin / 2});
        return path;
    }

    qreal SplitterItem::width() const { return m_width; }

    void SplitterItem::setWidth(const qreal &w) {
        m_width = w;
        update();
    }

    qreal SplitterItem::height() const {
        if (this->isVisible())
            return m_lineHeight + m_margin * 2;
        return m_margin / 2;
    }

    qreal SplitterItem::deltaY() const {
        if (this->isVisible())
            return m_lineHeight + m_margin + 1;
        return m_margin / 2;
    }

    void SplitterItem::setMargin(const qreal &margin) {
        if (m_margin != margin) {
            m_margin = margin;
            update();
        }
    }

    qreal SplitterItem::margin() const { return m_margin; }

    void SplitterItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_pen.color());
        painter->drawRect(QRectF(m_margin, m_margin, m_width - m_margin, m_lineHeight));
    }

    void SplitterItem::setQss() {
        const auto penValue = QssParser::propertyValue(m_view, "splitterPen");
        const auto pens = QssParser::parsePens(penValue, 1);
        if (!pens.isEmpty())
            m_pen = pens[0];
    }
} // namespace FillLyric
