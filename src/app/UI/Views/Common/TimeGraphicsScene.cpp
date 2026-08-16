#include "TimeGraphicsScene.h"
#include "TimeGridView.h"
#include "TimeIndicatorView.h"
#include "Global/AppGlobal.h"

TimeGraphicsScene::TimeGraphicsScene(QObject *parent) : QGraphicsScene(parent) {
    setSceneRect(0, 0, m_sceneSize.width(), m_sceneSize.height());
}

QSizeF TimeGraphicsScene::sceneBaseSize() const {
    return m_sceneSize;
}

void TimeGraphicsScene::setSceneBaseSize(const QSizeF &size) {
    m_sceneSize = size;
    updateSceneRect();
    emit baseSizeChanged(size);
}

void TimeGraphicsScene::addCommonItem(IScalableItem *item) {
    if (auto graphicsItem = dynamic_cast<QGraphicsItem *>(item)) {
        addItem(graphicsItem);
        m_items.append(item);
        item->setScaleXY(scaleX(), scaleY());
        item->setVisibleRect(visibleRect());
    } else
        qCritical() << "TimeGraphicsScene::addScalableItem: item is not QGraphicsItem";
}

void TimeGraphicsScene::removeCommonItem(IScalableItem *item) {
    removeItem(dynamic_cast<QGraphicsItem *>(item));
    m_items.removeOne(item);
}

void TimeGraphicsScene::addTimeGrid(TimeGridView *item) {
    item->setZValue(-1);
    addCommonItem(item);
}

void TimeGraphicsScene::addTimeIndicator(TimeIndicatorView *item) {
    item->setZValue(100);
    addCommonItem(item);
}

void TimeGraphicsScene::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
}

void TimeGraphicsScene::setLeftMarginPx(const int px) {
    if (m_leftMarginPx == px)
        return;
    m_leftMarginPx = px;
    updateSceneRect();
}

int TimeGraphicsScene::leftMarginPx() const {
    return m_leftMarginPx;
}

void TimeGraphicsScene::updateSceneRect() {
    auto scaledWidth = m_sceneSize.width() * scaleX() + m_leftMarginPx;
    auto scaledHeight = m_sceneSize.height() * scaleY();
    setSceneRect(0, 0, scaledWidth, scaledHeight);
}

void TimeGraphicsScene::afterSetScale() {
    updateSceneRect();
    for (auto item : m_items)
        item->setScaleXY(scaleX(), scaleY());
}

void TimeGraphicsScene::afterSetVisibleRect() {
    for (auto item : m_items)
        item->setVisibleRect(visibleRect());
}

void TimeGraphicsScene::setSceneLength(int tick) {
    setSceneBaseSize(QSizeF(tick * m_pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote, sceneBaseSize().height()));
}
