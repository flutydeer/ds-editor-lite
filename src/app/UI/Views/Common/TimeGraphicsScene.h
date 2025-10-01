//
// Created by fluty on 2024/2/10.
//

#ifndef TIMEGRAPHICSSCENE_H
#define TIMEGRAPHICSSCENE_H

#include "ScrollBarView.h"
#include "UI/Utils/IScalableItem.h"

#include <QGraphicsScene>

class CommonGraphicsLayer;
class IScalableItem;
class AbstractGraphicsRectItem;
class TimeGridView;
class TimeIndicatorView;

class TimeGraphicsScene : public QGraphicsScene, public IScalableItem {
    Q_OBJECT

public:
    explicit TimeGraphicsScene(QObject *parent = nullptr);

    [[nodiscard]] QSizeF sceneBaseSize() const;
    void setSceneBaseSize(const QSizeF &size);
    void addCommonItem(IScalableItem *item);
    void removeCommonItem(IScalableItem *item);

    void addTimeGrid(TimeGridView *item);
    void addTimeIndicator(TimeIndicatorView *item);
    void setPixelsPerQuarterNote(int px);
signals:
    void baseSizeChanged(const QSizeF &size);

protected:
    virtual void updateSceneRect();
    void afterSetScale() override;
    void afterSetVisibleRect() override;

private:
    friend class TimeGraphicsView;
    [[nodiscard]] ScrollBarView *horizontalBar();
    [[nodiscard]] ScrollBarView *verticalBar();
    void setHorizontalBarVisibility(bool visible);
    void setVerticalBarVisibility(bool visible);
    void setSceneLength(int tick);
    using QGraphicsScene::addItem;
    using QGraphicsScene::removeItem;

    QSizeF m_sceneSize = QSizeF(1920, 1080);
    QList<IScalableItem *> m_items;
    ScrollBarView m_hBar = ScrollBarView();
    ScrollBarView m_vBar = ScrollBarView(Qt::Vertical);
    bool m_hBarAdded = false;
    bool m_vBarAdded = false;
    int m_pixelsPerQuarterNote = 32;
};

#endif // TIMEGRAPHICSSCENE_H
