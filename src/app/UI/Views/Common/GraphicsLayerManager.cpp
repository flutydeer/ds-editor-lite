//
// Created by fluty on 24-3-10.
//

#include "GraphicsLayerManager.h"

#include "CommonGraphicsLayer.h"
#include "AbstractGraphicsRectItem.h"
#include "TimeGraphicsScene.h"

GraphicsLayerManager::GraphicsLayerManager(TimeGraphicsScene *scene) : m_scene(scene) {
}

void GraphicsLayerManager::addLayer(CommonGraphicsLayer *layer) {
    m_layers.append(layer);
    setLayerZValue(m_layers.count(), layer);
}

void GraphicsLayerManager::removeLayer(CommonGraphicsLayer *layer) {
    const auto index = m_layers.indexOf(layer);
    m_layers.removeAt(index);

    for (auto i = index; i < m_layers.count(); i++) {
        const auto curLayer = m_layers.at(i);
        setLayerZValue(i, curLayer);
    }
}

qsizetype GraphicsLayerManager::layerZValue(const CommonGraphicsLayer *layer) {
    return layer->layerZValue;
}

void GraphicsLayerManager::setLayerZValue(const qsizetype z, CommonGraphicsLayer *layer) {
    layer->layerZValue = z;
    updateItemsZValue(layer);
}

const QList<AbstractGraphicsRectItem *> &GraphicsLayerManager::items(CommonGraphicsLayer *layer) {
    return layer->items;
}

QList<AbstractGraphicsRectItem *> GraphicsLayerManager::allItems(CommonGraphicsLayer *layer) {
    QList<AbstractGraphicsRectItem *> items;
    for (const auto item : layer->backgroundItems)
        items.append(item);
    for (const auto item : layer->items)
        items.append(item);
    return items;
}

void GraphicsLayerManager::addBackgroundItem(AbstractGraphicsRectItem *item,
                                             CommonGraphicsLayer *layer) const {
    item->setZValue(layer->layerZValue * 2);
    layer->backgroundItems.append(item);
    m_scene->addCommonItem(item);
}

void GraphicsLayerManager::addItem(AbstractGraphicsRectItem *item,
                                   CommonGraphicsLayer *layer) const {
    item->setZValue(layer->layerZValue * 2 + 1);
    layer->items.append(item);
    m_scene->addCommonItem(item);
}

void GraphicsLayerManager::removeItem(AbstractGraphicsRectItem *item,
                                      CommonGraphicsLayer *layer) const {
    layer->items.removeOne(item);
    m_scene->removeCommonItem(item);
}

void GraphicsLayerManager::clearItems(CommonGraphicsLayer *layer) const {
    for (const auto item : layer->items)
        removeItem(item, layer);
}

void GraphicsLayerManager::destroyItems(CommonGraphicsLayer *layer) const {
    for (const auto item : layer->items) {
        removeItem(item, layer);
        delete item;
    }
}

void GraphicsLayerManager::updateItemsZValue(CommonGraphicsLayer *layer) {
    for (const auto item : layer->backgroundItems)
        item->setZValue(layer->layerZValue * 2);

    for (const auto item : layer->items)
        item->setZValue(layer->layerZValue * 2 + 1);
}