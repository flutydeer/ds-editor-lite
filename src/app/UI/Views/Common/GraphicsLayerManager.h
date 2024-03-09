//
// Created by fluty on 24-3-10.
//

#ifndef GRAPHICSLAYERMANAGER_H
#define GRAPHICSLAYERMANAGER_H

#include <QList>

class CommonGraphicsRectItem;
class CommonGraphicsLayer;
class CommonGraphicsScene;

class GraphicsLayerManager {
public:
    explicit GraphicsLayerManager(CommonGraphicsScene *scene);

    // void insertLayer(qsizetype zValue, CommonGraphicsLayer *layer);
    void addLayer(CommonGraphicsLayer *layer);
    void removeLayer(CommonGraphicsLayer *layer);

    [[nodiscard]] qsizetype layerZValue(CommonGraphicsLayer *layer) const;
    void setLayerZValue(qsizetype z, CommonGraphicsLayer *layer);
    [[nodiscard]] const QList<CommonGraphicsRectItem *> &items(CommonGraphicsLayer *layer) const;
    [[nodiscard]] QList<CommonGraphicsRectItem *> allItems(CommonGraphicsLayer *layer) const;
    void addBackgroundItem(CommonGraphicsRectItem *item, CommonGraphicsLayer *layer);
    void addItem(CommonGraphicsRectItem *item, CommonGraphicsLayer *layer);
    // void removeBackgroundItem(CommonGraphicsRectItem *item);
    void removeItem(CommonGraphicsRectItem *item, CommonGraphicsLayer *layer);
    void clearItems(CommonGraphicsLayer *layer);
    void destroyItems(CommonGraphicsLayer *layer);

private:
    CommonGraphicsScene *m_scene;
    QList<CommonGraphicsLayer *> m_layers;

    void updateItemsZValue(CommonGraphicsLayer *layer);
};



#endif // GRAPHICSLAYERMANAGER_H
