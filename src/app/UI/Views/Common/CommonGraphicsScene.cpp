//
// Created by fluty on 2024/1/23.
//

#include "CommonGraphicsScene.h"

#include "CommonGraphicsLayer.h"
#include "CommonGraphicsRectItem.h"

CommonGraphicsScene::CommonGraphicsScene() {
    setSceneRect(0, 0, m_sceneSize.width(), m_sceneSize.height());
}
QSizeF CommonGraphicsScene::sceneSize() const {
    return m_sceneSize;
}
void CommonGraphicsScene::setSceneSize(const QSizeF &size) {
    m_sceneSize = size;
    updateSceneRect();
}
void CommonGraphicsScene::addCommonItem(IScalableItem *item) {
    item->setScaleXY(scaleX(), scaleY());
    item->setVisibleRect(visibleRect());
    if (auto graphicsItem = dynamic_cast<QGraphicsItem *>(item)) {
        addItem(graphicsItem);
        m_items.append(item);
    } else
        qDebug() << "CommonGraphicsScene::addScalableItem: item is not QGraphicsItem";
}
void CommonGraphicsScene::removeCommonItem(IScalableItem *item) {
    removeItem(dynamic_cast<QGraphicsItem *>(item));
    m_items.removeOne(item);
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