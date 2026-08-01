#ifndef COMMONGRAPHICSLAYER_H
#define COMMONGRAPHICSLAYER_H

#include <QObject>

class GraphicsLayerManager;
class AbstractGraphicsRectItem;
class CommonGraphicsScene;

class CommonGraphicsLayer : public QObject {
    Q_OBJECT

public:
    explicit CommonGraphicsLayer(QObject *parent = nullptr) : QObject(parent) {
    }

protected:
    friend class GraphicsLayerManager;
    qsizetype layerZValue = 0;
    QList<AbstractGraphicsRectItem *> backgroundItems;
    QList<AbstractGraphicsRectItem *> items;
};



#endif // COMMONGRAPHICSLAYER_H
