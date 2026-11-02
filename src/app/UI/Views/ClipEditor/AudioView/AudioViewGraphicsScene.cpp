#include "AudioViewGraphicsScene.h"

#include "Global/AppGlobal.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <algorithm>

AudioViewGraphicsScene::AudioViewGraphicsScene() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    setSceneBaseSize(QSizeF(ClipEditorGlobal::pixelsPerQuarterNote, 1.0));
}

void AudioViewGraphicsScene::setViewportHeight(const double height) {
    const auto target = std::max(1.0, height);
    if (qFuzzyCompare(sceneBaseSize().height(), target))
        return;
    setSceneBaseSize(QSizeF(sceneBaseSize().width(), target));
}

void AudioViewGraphicsScene::setContentLength(const int ticks) {
    const auto width = static_cast<double>(std::max(0, ticks)) *
                       ClipEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    setSceneBaseSize(QSizeF(std::max(1.0, width), sceneBaseSize().height()));
}
