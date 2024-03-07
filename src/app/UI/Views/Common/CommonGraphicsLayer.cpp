//
// Created by fluty on 24-3-7.
//

#include "CommonGraphicsLayer.h"

#include "CommonGraphicsScene.h"
#include "CommonGraphicsRectItem.h"

CommonGraphicsLayer::CommonGraphicsLayer(QObject *parent) : QObject(parent) {
}
void CommonGraphicsLayer::setScene(CommonGraphicsScene *scene) {
    if (m_scene == scene)
        return;

    if (m_scene) {
        for (auto item : allItems())
            m_scene->removeCommonItem(item);
    }
    m_scene = scene;
    if (m_scene) {
        updateItemsZValue();
        for (auto item : allItems())
            m_scene->addCommonItem(item);
    }
}
qsizetype CommonGraphicsLayer::layerZValue() const {
    return m_layerZValue;
}
void CommonGraphicsLayer::setLayerZValue(qsizetype z) {
    m_layerZValue = z;
    updateItemsZValue();
}
void CommonGraphicsLayer::addBackgroundItem(CommonGraphicsRectItem *item) {
    m_backgroundItems.append(item);
    if (m_scene) {
        item->setZValue(m_layerZValue * 2);
        m_scene->addCommonItem(item);
    }
}
void CommonGraphicsLayer::addItem(CommonGraphicsRectItem *item) {
    m_items.append(item);
    if (m_scene) {
        item->setZValue(m_layerZValue * 2 + 1);
        m_scene->addCommonItem(item);
    }
}
void CommonGraphicsLayer::removeBackgroundItem(CommonGraphicsRectItem *item) {
    m_backgroundItems.removeOne(item);
    if (m_scene)
        m_scene->removeCommonItem(item);
}
void CommonGraphicsLayer::removeItem(CommonGraphicsRectItem *item) {
    m_items.removeOne(item);
    if (m_scene)
        m_scene->removeCommonItem(item);
}
void CommonGraphicsLayer::setItemsVisibility(bool isVisible) const {
    for (auto item : allItems())
        item->setVisible(isVisible);
}
const QList<CommonGraphicsRectItem *> &CommonGraphicsLayer::items() const {
    return m_items;
}
void CommonGraphicsLayer::clearItems() {
    for (const auto item : m_items)
        removeItem(item);
}
void CommonGraphicsLayer::destroyItems() {
    for (const auto item : m_items) {
        removeItem(item);
        delete item;
    }
}
QList<CommonGraphicsRectItem *> CommonGraphicsLayer::allItems() const {
    QList<CommonGraphicsRectItem *> items;
    for (const auto item : m_backgroundItems)
        items.append(item);
    for (const auto item : m_items)
        items.append(item);
    return items;
}
void CommonGraphicsLayer::updateItemsZValue() {
    for (auto item : m_backgroundItems)
        item->setZValue(m_layerZValue * 2);

    for (auto item : m_items)
        item->setZValue(m_layerZValue * 2 + 1);
}