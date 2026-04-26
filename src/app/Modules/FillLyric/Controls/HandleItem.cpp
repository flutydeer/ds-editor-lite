#include "Modules/FillLyric/Controls/HandleItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include "Modules/FillLyric/Utils/QssParser.h"

namespace FillLyric
{
    HandleItem::HandleItem(QGraphicsView *view, QGraphicsItem *parent) : QGraphicsItem(parent), m_view(view) {
        setFlag(ItemIsSelectable);
        this->setAcceptHoverEvents(true);
        this->setQss();
    }

    HandleItem::~HandleItem() = default;

    static void handleSelection(HandleItem *self, QGraphicsSceneMouseEvent *event) {
        if (event->button() == Qt::RightButton && self->scene()->selectedItems().size() > 1 && self->isSelected()) {
            event->accept();
            return;
        }

        if (!(event->modifiers() & Qt::ControlModifier)) {
            for (const auto item : self->scene()->selectedItems()) {
                item->setSelected(false);
            }
        }
        Q_EMIT self->selectAll();
        self->setSelected(true);
    }

    void HandleItem::mousePressEvent(QGraphicsSceneMouseEvent *event) { handleSelection(this, event); }

    void HandleItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { handleSelection(this, event); }

    void HandleItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
        Q_EMIT contextMenuRequested(event->screenPos());
    }

    QRectF HandleItem::boundingRect() const { return {0, 0, width(), height()}; }

    QPainterPath HandleItem::shape() const {
        QPainterPath path;
        path.addRect({m_margin, m_margin, width() - m_margin * 2, height() - m_margin * 2});
        return path;
    }

    void HandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
        int flag = 0;
        if (option->state & QStyle::State_MouseOver)
            flag = Hovered;
        if (option->state & QStyle::State_Selected)
            flag = Selected;

        const auto boxRect = QRectF(m_margin, m_margin * 1.5, width() - m_margin * 2, height() - m_margin * 3);

        painter->setPen(Qt::NoPen);
        painter->setBrush(m_backgroundBrush[flag]);
        painter->drawRoundedRect(boxRect, m_margin * 0.5, m_margin * 0.5);
    }

    void HandleItem::setWidth(const qreal &w) { m_width = w; }

    qreal HandleItem::width() const { return m_width; }

    void HandleItem::setHeight(const qreal &h) { m_height = h; }

    qreal HandleItem::height() const { return m_height; }

    void HandleItem::setMargin(const qreal &margin) { m_margin = margin; }

    qreal HandleItem::margin() const { return m_margin; }

    void HandleItem::setQss() {
        const auto brushValue = QssParser::propertyValue(m_view, "handleBackgroundBrush");
        const auto brushes = QssParser::parseBrushes(brushValue, 3);
        if (brushes.size() == 3) {
            for (int i = 0; i < 3; i++)
                m_backgroundBrush[i] = brushes[i];
        }
    }
} // namespace FillLyric
