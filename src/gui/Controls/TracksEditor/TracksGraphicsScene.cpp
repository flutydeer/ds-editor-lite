//
// Created by fluty on 2023/11/14.
//

#include <QDebug>

#include "TracksGraphicsScene.h"
#include "TracksEditorGlobal.h"

using namespace TracksEditorGlobal;

TracksGraphicsScene::TracksGraphicsScene() {
    setSceneSize(QSizeF(1920 / 480 * pixelsPerQuarterNote * 100, 2000));
}
void TracksGraphicsScene::onViewResized(QSize size) {
    m_graphicsViewSize = size;
    updateSceneRect();
}
void TracksGraphicsScene::onTrackCountChanged(int count) {
    m_trackCount = count;
    updateSceneRect();
}
void TracksGraphicsScene::updateSceneRect() {
    // CommonGraphicsScene::updateSceneRect();
    auto targetSceneWidth = sceneSize().width() * scaleX();
    // auto targetSceneHeight = sceneSize().height() * scaleY();
    auto totalTrackHeight = m_trackCount * trackHeight * scaleY();
    auto viewHeight = m_graphicsViewSize.height();

    auto targetSceneHeight = totalTrackHeight;
    // Adjust scene height to match view when track count or scaleY is too small
    if (totalTrackHeight < viewHeight) {
        // auto paddingBottom = viewHeight - totalTrackHeight;
        targetSceneHeight = viewHeight;
    }
    setSceneRect(0, 0, targetSceneWidth, targetSceneHeight);
}