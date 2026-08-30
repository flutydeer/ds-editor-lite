#include "TracksGraphicsScene.h"

#include "Global/TracksEditorGlobal.h"
#include "Global/AppGlobal.h"

TracksGraphicsScene::TracksGraphicsScene() {
    setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
}

int TracksGraphicsScene::trackIndexAt(const double sceneY) const {
    auto yToTrackIndex = [&](const double y) {
        return static_cast<int>(y / (TracksEditorGlobal::trackHeight * scaleY()));
    };
    auto index = yToTrackIndex(sceneY);
    if (index >= m_trackCount)
        index = -1;
    return index;
}

bool TracksGraphicsScene::isAppendSlotAt(const double sceneY) const {
    if (sceneY < 0)
        return false;
    const auto unit = sceneY / (TracksEditorGlobal::trackHeight * scaleY());
    return unit >= m_trackCount && unit < m_trackCount + 1.0;
}

int TracksGraphicsScene::tickAt(const double sceneX) const {
    return static_cast<int>(AppGlobal::ticksPerQuarterNote * (sceneX - leftMarginPx()) / scaleX() /
                            TracksEditorGlobal::pixelsPerQuarterNote);
}

void TracksGraphicsScene::onViewResized(const QSize size) {
    m_graphicsViewSize = size;
    updateSceneRect();
}

void TracksGraphicsScene::onTrackCountChanged(const int count) {
    m_trackCount = count;
    updateSceneRect();
}

void TracksGraphicsScene::updateSceneRect() {
    const auto targetSceneWidth = sceneBaseSize().width() * scaleX() + leftMarginPx();
    // One extra row for the virtual append slot at the bottom of the canvas,
    // so the slot stays reachable by scrolling once the tracks fill the view.
    const auto totalTrackHeight = (m_trackCount + 1) * TracksEditorGlobal::trackHeight * scaleY();
    const auto viewHeight = m_graphicsViewSize.height();

    auto targetSceneHeight = totalTrackHeight;
    // Adjust scene height to match view when track count or scaleY is too small
    if (totalTrackHeight < viewHeight) {
        targetSceneHeight = viewHeight;
    }
    setSceneRect(0, 0, targetSceneWidth, targetSceneHeight);
}