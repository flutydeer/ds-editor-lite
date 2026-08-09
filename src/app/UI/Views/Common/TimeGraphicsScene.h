#ifndef TIMEGRAPHICSSCENE_H
#define TIMEGRAPHICSSCENE_H

#include <lite/GUI/Base/IScalableItem.h>

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
    void setSceneLength(int tick);
    using QGraphicsScene::addItem;
    using QGraphicsScene::removeItem;

    QSizeF m_sceneSize = QSizeF(1920, 1080);
    QList<IScalableItem *> m_items;
    int m_pixelsPerQuarterNote = 32;
};

#endif // TIMEGRAPHICSSCENE_H
