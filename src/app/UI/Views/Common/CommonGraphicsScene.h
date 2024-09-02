//
// Created by fluty on 2024/1/23.
//

#ifndef COMMONGRAPHICSSCENE_H
#define COMMONGRAPHICSSCENE_H

#include "ScrollBarGraphicsItem.h"
#include "UI/Utils/IScalableItem.h"

#include <QGraphicsScene>

class CommonGraphicsLayer;
class IScalableItem;
class CommonGraphicsRectItem;

class CommonGraphicsScene : public QGraphicsScene, public IScalableItem {
    Q_OBJECT
public:
    explicit CommonGraphicsScene(QObject *parent = nullptr);
    ~CommonGraphicsScene() override = default;
    [[nodiscard]] QSizeF sceneBaseSize() const;
    void setSceneBaseSize(const QSizeF &size);
    void addCommonItem(IScalableItem *item);
    void removeCommonItem(IScalableItem *item);
    [[nodiscard]] ScrollBarGraphicsItem *hBar();
    [[nodiscard]] ScrollBarGraphicsItem *vBar();
    void setHBarVisibility(bool visible);
    void setVBarVisibility(bool visible);

signals:
    void baseSizeChanged(const QSizeF &size);

protected:
    virtual void updateSceneRect();
    void afterSetScale() override;
    void afterSetVisibleRect() override;

private:
    using QGraphicsScene::addItem;
    using QGraphicsScene::removeItem;
    QSizeF m_sceneSize = QSizeF(1920, 1080);
    QList<IScalableItem *> m_items;
    ScrollBarGraphicsItem m_hBar = ScrollBarGraphicsItem();
    ScrollBarGraphicsItem m_vBar = ScrollBarGraphicsItem(Qt::Vertical);
    bool m_hBarAdded = false;
    bool m_vBarAdded = false;
};

#endif // COMMONGRAPHICSSCENE_H
