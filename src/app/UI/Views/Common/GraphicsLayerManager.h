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

    static qsizetype layerZValue(const CommonGraphicsLayer *layer);
    static void setLayerZValue(qsizetype z, CommonGraphicsLayer *layer);
    static const QList<AbstractGraphicsRectItem *> &items(CommonGraphicsLayer *layer);
    static QList<AbstractGraphicsRectItem *> allItems(CommonGraphicsLayer *layer);
    void addBackgroundItem(AbstractGraphicsRectItem *item, CommonGraphicsLayer *layer) const;
    void addItem(AbstractGraphicsRectItem *item, CommonGraphicsLayer *layer) const;
    // void removeBackgroundItem(CommonGraphicsRectItem *item);
    void removeItem(AbstractGraphicsRectItem *item, CommonGraphicsLayer *layer) const;
    void clearItems(CommonGraphicsLayer *layer) const;
    void destroyItems(CommonGraphicsLayer *layer) const;

private:
    TimeGraphicsScene *m_scene;
    QList<CommonGraphicsLayer *> m_layers;

    static void updateItemsZValue(CommonGraphicsLayer *layer);
};



#endif // GRAPHICSLAYERMANAGER_H
