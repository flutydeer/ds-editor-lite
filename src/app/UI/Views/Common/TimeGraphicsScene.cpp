//
// Created by fluty on 2024/2/10.
//

#include "TimeGraphicsScene.h"
#include "TimeGridView.h"
#include "TimeIndicatorView.h"

TimeGraphicsScene::TimeGraphicsScene(QObject *parent) : QGraphicsScene(parent) {
    setSceneRect(0, 0, m_sceneSize.width(), m_sceneSize.height());

    m_hBar.setZValue(105);
    addCommonItem(&m_hBar);
    m_hBarAdded = true;
    m_vBar.setZValue(105);
    addCommonItem(&m_vBar);
    m_vBarAdded = true;
    connect(this, &TimeGraphicsScene::sceneRectChanged, this, [&] {
        m_hBar.updateRectAndPos();
        m_vBar.updateRectAndPos();
    });
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
    item->setScaleXY(scaleX(), scaleY());
    item->setVisibleRect(visibleRect());
    if (const auto graphicsItem = dynamic_cast<QGraphicsItem *>(item)) {
        addItem(graphicsItem);
        m_items.append(item);
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

void TimeGraphicsScene::setPixelsPerQuarterNote(const int px) {
    m_pixelsPerQuarterNote = px;
}

void TimeGraphicsScene::updateSceneRect() {
    const auto scaledWidth = m_sceneSize.width() * scaleX();
    const auto scaledHeight = m_sceneSize.height() * scaleY();
    setSceneRect(0, 0, scaledWidth, scaledHeight);
}

void TimeGraphicsScene::afterSetScale() {
    updateSceneRect();
    for (const auto item : m_items)
        item->setScaleXY(scaleX(), scaleY());
}

void TimeGraphicsScene::afterSetVisibleRect() {
    for (const auto item : m_items)
        item->setVisibleRect(visibleRect());
}

ScrollBarView *TimeGraphicsScene::horizontalBar() {
    return &m_hBar;
}

ScrollBarView *TimeGraphicsScene::verticalBar() {
    return &m_vBar;
}

void TimeGraphicsScene::setHorizontalBarVisibility(const bool visible) {
    if (visible) {
        if (!m_hBarAdded) {
            addCommonItem(&m_hBar);
            m_hBarAdded = true;
        }
    } else {
        if (m_hBarAdded) {
            removeCommonItem(&m_hBar);
            m_hBarAdded = false;
        }
    }
}

void TimeGraphicsScene::setVerticalBarVisibility(const bool visible) {
    if (visible) {
        if (!m_vBarAdded) {
            addCommonItem(&m_vBar);
            m_vBarAdded = true;
        }
    } else {
        if (m_vBarAdded) {
            removeCommonItem(&m_vBar);
            m_vBarAdded = false;
        }
    }
}

void TimeGraphicsScene::setSceneLength(const int tick) {
    setSceneBaseSize(QSizeF(tick * m_pixelsPerQuarterNote / 480.0, sceneBaseSize().height()));
}
