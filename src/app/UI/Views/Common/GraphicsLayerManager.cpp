//
// Created by fluty on 24-3-10.
//

#include "GraphicsLayerManager.h"

#include "CommonGraphicsLayer.h"
#include "CommonGraphicsRectItem.h"
#include "CommonGraphicsScene.h"

GraphicsLayerManager::GraphicsLayerManager(CommonGraphicsScene *scene) : m_scene(scene) {
}
void GraphicsLayerManager::addLayer(CommonGraphicsLayer *layer) {
    m_layers.append(layer);
    setLayerZValue(m_layers.count(), layer);
}
void GraphicsLayerManager::removeLayer(CommonGraphicsLayer *layer) {
    auto index = m_layers.indexOf(layer);
    m_layers.removeAt(index);

    for (auto i = index; i < m_layers.count(); i++) {
        auto curLayer = m_layers.at(i);
        setLayerZValue(i, curLayer);
    }
}
qsizetype GraphicsLayerManager::layerZValue(CommonGraphicsLayer *layer) const {
    return layer->layerZValue;
}
void GraphicsLayerManager::setLayerZValue(qsizetype z, CommonGraphicsLayer *layer) {
    layer->layerZValue = z;
    updateItemsZValue(layer);
}
const QList<CommonGraphicsRectItem *> &
    GraphicsLayerManager::items(CommonGraphicsLayer *layer) const {
    return layer->items;
}
QList<CommonGraphicsRectItem *> GraphicsLayerManager::allItems(CommonGraphicsLayer *layer) const {
    QList<CommonGraphicsRectItem *> items;
    for (const auto item : layer->backgroundItems)
        items.append(item);
    for (const auto item : layer->items)
        items.append(item);
    return items;
}
void GraphicsLayerManager::addBackgroundItem(CommonGraphicsRectItem *item,
                                             CommonGraphicsLayer *layer) {
    item->setZValue(layer->layerZValue * 2);
    layer->backgroundItems.append(item);
    m_scene->addCommonItem(item);
}
void GraphicsLayerManager::addItem(CommonGraphicsRectItem *item, CommonGraphicsLayer *layer) {
    item->setZValue(layer->layerZValue * 2 + 1);
    layer->items.append(item);
    m_scene->addCommonItem(item);
}
void GraphicsLayerManager::removeItem(CommonGraphicsRectItem *item, CommonGraphicsLayer *layer) {
    layer->items.removeOne(item);
    m_scene->removeCommonItem(item);
}
void GraphicsLayerManager::clearItems(CommonGraphicsLayer *layer) {
    for (const auto item : layer->items)
        removeItem(item, layer);
}
void GraphicsLayerManager::destroyItems(CommonGraphicsLayer *layer) {
    for (const auto item : layer->items) {
        removeItem(item, layer);
        delete item;
    }
}
void GraphicsLayerManager::updateItemsZValue(CommonGraphicsLayer *layer) {
    for (auto item : layer->backgroundItems)
        item->setZValue(layer->layerZValue * 2);

    for (auto item : layer->items)
        item->setZValue(layer->layerZValue * 2 + 1);
}