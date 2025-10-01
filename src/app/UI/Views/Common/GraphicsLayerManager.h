//
// Created by fluty on 24-3-10.
//

#ifndef GRAPHICSLAYERMANAGER_H
#define GRAPHICSLAYERMANAGER_H

#include <QList>

class AbstractGraphicsRectItem;
class CommonGraphicsLayer;
class TimeGraphicsScene;

class GraphicsLayerManager {
public:
    explicit GraphicsLayerManager(TimeGraphicsScene *scene);

    // void insertLayer(qsizetype zValue, CommonGraphicsLayer *layer);
    void addLayer(CommonGraphicsLayer *layer);
    void removeLayer(CommonGraphicsLayer *layer);

    [[nodiscard]] qsizetype layerZValue(CommonGraphicsLayer *layer) const;
    void setLayerZValue(qsizetype z, CommonGraphicsLayer *layer);
    [[nodiscard]] const QList<AbstractGraphicsRectItem *> &items(CommonGraphicsLayer *layer) const;
    [[nodiscard]] QList<AbstractGraphicsRectItem *> allItems(CommonGraphicsLayer *layer) const;
    void addBackgroundItem(AbstractGraphicsRectItem *item, CommonGraphicsLayer *layer);
    void addItem(AbstractGraphicsRectItem *item, CommonGraphicsLayer *layer);
    // void removeBackgroundItem(CommonGraphicsRectItem *item);
    void removeItem(AbstractGraphicsRectItem *item, CommonGraphicsLayer *layer);
    void clearItems(CommonGraphicsLayer *layer);
    void destroyItems(CommonGraphicsLayer *layer);

private:
    TimeGraphicsScene *m_scene;
    QList<CommonGraphicsLayer *> m_layers;

    void updateItemsZValue(CommonGraphicsLayer *layer);
};



#endif // GRAPHICSLAYERMANAGER_H
