#include "HandleItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>

namespace FillLyric {
    HandleItem::HandleItem(QGraphicsView *view, QGraphicsItem *parent)
        : QGraphicsItem(parent), m_view(view) {
        setFlag(ItemIsSelectable);
        this->setAcceptHoverEvents(true);

        this->setQss();
    }

    HandleItem::~HandleItem() = default;

    void HandleItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
        if (!(event->modifiers() & Qt::ControlModifier)) {
            for (const auto item : scene()->selectedItems()) {
                item->setSelected(false);
            }
        }
        Q_EMIT selectAll();
        this->setSelected(true);
    }

    void HandleItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
        if (!(event->modifiers() & Qt::ControlModifier)) {
            for (const auto item : scene()->selectedItems()) {
                item->setSelected(false);
            }
        }
        Q_EMIT selectAll();
        this->setSelected(true);
    }

    QRectF HandleItem::boundingRect() const {
        return {0, 0, width(), height()};
    }

    QPainterPath HandleItem::shape() const {
        QPainterPath path;
        path.addRect({m_margin, m_margin, width() - m_margin * 2, height() - m_margin * 2});
        return path;
    }

    void HandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                           QWidget *widget) {
        int flag = 0;
        if (option->state & QStyle::State_MouseOver)
            flag = Hovered;
        if (option->state & QStyle::State_Selected)
            flag = Selected;

        const auto boxRect =
            QRectF(m_margin, m_margin * 1.5, width() - m_margin * 2, height() - m_margin * 3);

        painter->setPen(Qt::NoPen);
        painter->setBrush(m_backgroundBrush[flag]);
        painter->drawRoundedRect(boxRect, m_margin * 0.5, m_margin * 0.5);
    }

    void HandleItem::setWidth(const qreal &w) {
        mW = w;
    }

    qreal HandleItem::width() const {
        return mW;
    }

    void HandleItem::setHeight(const qreal &h) {
        mH = h;
    }

    qreal HandleItem::height() const {
        return mH;
    }

    qreal HandleItem::deltaX() const {
        return width() - margin();
    }

    void HandleItem::setMargin(const qreal &margin) {
        m_margin = margin;
    }

    qreal HandleItem::margin() const {
        return m_margin;
    }

    void HandleItem::setQss() {
        const auto cellBackBrush = m_view->property("handleBackgroundBrush").toStringList()[1];
        if (!cellBackBrush.isEmpty()) {
            const auto brushList = cellBackBrush.split('|');
            if (brushList.size() == 3) {
                for (int i = 0; i < 3; i++) {
                    if (brushList[i] == "NoBrush")
                        m_backgroundBrush[i] = QBrush(Qt::NoBrush);
                    else {
                        const auto colorStr = brushList[i].split(',');
                        const QVector<int> colorValue = {colorStr[0].toInt(), colorStr[1].toInt(),
                                                         colorStr[2].toInt(), colorStr[3].toInt()};

                        if (colorValue.size() == 4) {
                            m_backgroundBrush[i] =
                                QBrush(QColor(colorStr[0].toInt(), colorStr[1].toInt(),
                                              colorStr[2].toInt(), colorStr[3].toInt()));
                        }
                    }
                }
            }
        }
    }
} // FillLyric