//
// Created by fluty on 24-8-21.
//
#include "ParamEditorGraphicsScene.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/Log.h"

ParamEditorGraphicsScene::ParamEditorGraphicsScene(QObject *parent) : TimeGraphicsScene(parent) {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

void ParamEditorGraphicsScene::onViewResized(QSize size) {
    m_viewSize = size;
    updateSceneRect();
}

void ParamEditorGraphicsScene::updateSceneRect() {
    // 参数编辑器场景填充整个视图
    auto targetSceneWidth = sceneBaseSize().width() * scaleX();
    auto targetRect = QRectF(0, 0, targetSceneWidth, m_viewSize.height());
    setSceneRect(targetRect);
}