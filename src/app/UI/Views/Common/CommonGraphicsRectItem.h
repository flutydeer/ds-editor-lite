//
// Created by fluty on 2024/1/26.
//

#ifndef COMMONGRAPHICSRECTITEM_H
#define COMMONGRAPHICSRECTITEM_H

#include <QGraphicsRectItem>

#include "UI/Utils/IScalableItem.h"

class CommonGraphicsRectItem : public QObject, public QGraphicsRectItem, public IScalableItem {
    Q_OBJECT
public:
    explicit CommonGraphicsRectItem(QGraphicsItem *parent = nullptr) : QGraphicsRectItem(parent){}

protected:
    virtual void updateRectAndPos() = 0;
    void afterSetScale() override {
        updateRectAndPos();
    }
    void afterSetVisibleRect() override {
        updateRectAndPos();
    }
};

#endif // COMMONGRAPHICSRECTITEM_H
