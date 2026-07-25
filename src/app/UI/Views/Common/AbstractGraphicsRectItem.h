//
// Created by fluty on 2024/1/26.
//

#ifndef ABSTRACTGRAPHICSRECTITEM_H
#define ABSTRACTGRAPHICSRECTITEM_H

#include <QGraphicsRectItem>

#include <lite/GUI/Base/IScalableItem.h>

class AbstractGraphicsRectItem : public QObject, public QGraphicsRectItem, public IScalableItem {
    Q_OBJECT
public:
    explicit AbstractGraphicsRectItem(QGraphicsItem *parent = nullptr) : QGraphicsRectItem(parent) {
    }

protected:
    virtual void updateRectAndPos() = 0;

    void afterSetScale() override {
        updateRectAndPos();
    }

    void afterSetVisibleRect() override {
        updateRectAndPos();
    }
};

#endif // ABSTRACTGRAPHICSRECTITEM_H
