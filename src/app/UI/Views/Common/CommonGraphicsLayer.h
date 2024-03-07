//
// Created by fluty on 24-3-7.
//

#ifndef COMMONGRAPHICSLAYER_H
#define COMMONGRAPHICSLAYER_H

#include <QObject>

class CommonGraphicsRectItem;
class CommonGraphicsScene;

class CommonGraphicsLayer : public QObject {
    Q_OBJECT
public:
    explicit CommonGraphicsLayer(QObject *parent = nullptr);

    void setScene(CommonGraphicsScene *scene);
    qsizetype layerZValue() const;
    void setLayerZValue(qsizetype z);
    void addBackgroundItem(CommonGraphicsRectItem *item);
    void addItem(CommonGraphicsRectItem *item);
    void removeBackgroundItem(CommonGraphicsRectItem *item);
    void removeItem(CommonGraphicsRectItem *item);
    void setItemsVisibility(bool isVisible) const;
    [[nodiscard]] const QList<CommonGraphicsRectItem *> &items() const;
    void clearItems();
    void destroyItems();
    [[nodiscard]] QList<CommonGraphicsRectItem *> allItems() const;

private:
    CommonGraphicsScene *m_scene = nullptr;
    qsizetype m_layerZValue = 0;

    QList<CommonGraphicsRectItem *> m_backgroundItems;
    QList<CommonGraphicsRectItem *> m_items;

    void updateItemsZValue();
};



#endif // COMMONGRAPHICSLAYER_H
