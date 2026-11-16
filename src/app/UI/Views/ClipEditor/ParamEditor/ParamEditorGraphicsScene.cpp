#include "ParamEditorGraphicsScene.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include <lite/Support/Log.h>

ParamEditorGraphicsScene::ParamEditorGraphicsScene(QObject *parent) : TimeGraphicsScene(parent) {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

void ParamEditorGraphicsScene::onViewResized(const QSize size) {
    m_viewSize = size;
    updateSceneRect();
}

void ParamEditorGraphicsScene::updateSceneRect() {
    // Keep the parameter editor scene large enough to fill the view.
    const auto targetSceneWidth = sceneBaseSize().width() * scaleX();
    const auto targetRect = QRectF(0, 0, targetSceneWidth, m_viewSize.height());
    setSceneRect(targetRect);
}
