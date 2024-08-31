//
// Created by fluty on 2024/1/23.
//

#include "CommonGraphicsScene.h"

#include "CommonGraphicsRectItem.h"
#include "ScrollBarGraphicsItem.h"

CommonGraphicsScene::CommonGraphicsScene(QObject *parent) : QGraphicsScene(parent) {
    setSceneRect(0, 0, m_sceneSize.width(), m_sceneSize.height());

    m_hBar.setZValue(105);
    addCommonItem(&m_hBar);
    m_vBar.setZValue(105);
    addCommonItem(&m_vBar);
    connect(this, &CommonGraphicsScene::sceneRectChanged, this, [&] {
        m_hBar.updateRectAndPos();
        m_vBar.updateRectAndPos();
    });
}

QSizeF CommonGraphicsScene::sceneBaseSize() const {
    return m_sceneSize;
}

void CommonGraphicsScene::setSceneBaseSize(const QSizeF &size) {
    m_sceneSize = size;
    updateSceneRect();
    emit baseSizeChanged(size);
}

void CommonGraphicsScene::addCommonItem(IScalableItem *item) {
    item->setScaleXY(scaleX(), scaleY());
    item->setVisibleRect(visibleRect());
    if (auto graphicsItem = dynamic_cast<QGraphicsItem *>(item)) {
        addItem(graphicsItem);
        m_items.append(item);
    } else
        qCritical() << "CommonGraphicsScene::addScalableItem: item is not QGraphicsItem";
}

void CommonGraphicsScene::removeCommonItem(IScalableItem *item) {
    removeItem(dynamic_cast<QGraphicsItem *>(item));
    m_items.removeOne(item);
}

ScrollBarGraphicsItem *CommonGraphicsScene::hBar() {
    return &m_hBar;
}

ScrollBarGraphicsItem *CommonGraphicsScene::vBar() {
    return &m_vBar;
}

void CommonGraphicsScene::updateSceneRect() {
    auto scaledWidth = m_sceneSize.width() * scaleX();
    auto scaledHeight = m_sceneSize.height() * scaleY();
    setSceneRect(0, 0, scaledWidth, scaledHeight);
}

void CommonGraphicsScene::afterSetScale() {
    updateSceneRect();
    for (auto item : m_items)
        item->setScaleXY(scaleX(), scaleY());
}

void CommonGraphicsScene::afterSetVisibleRect() {
    for (auto item : m_items)
        item->setVisibleRect(visibleRect());
}