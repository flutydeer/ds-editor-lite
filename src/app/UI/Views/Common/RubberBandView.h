//
// Created by fluty on 24-8-26.
//

#ifndef RUBBERBANDVIEW_H
#define RUBBERBANDVIEW_H

#include "AbstractGraphicsRectItem.h"

class RubberBandView : public AbstractGraphicsRectItem {
public:
    enum class SelectMode { RectSelect, BeamSelect };
    explicit RubberBandView();
    void mouseDown(const QPointF &pos);
    void mouseMove(const QPointF &pos);
    void setSelectMode(SelectMode mode);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;

    QPointF m_mouseDownPos;
    QPointF m_currentMousePos;
    QPointF m_pos;
    QSizeF m_size;
    SelectMode m_selectMode = SelectMode::BeamSelect;
};



#endif // RUBBERBANDVIEW_H
