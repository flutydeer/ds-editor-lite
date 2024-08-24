//
// Created by fluty on 24-8-21.
//

#define CLASS_NAME "ParamEditorGraphicsScene"

#include "ParamEditorGraphicsScene.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/Log.h"

using namespace ClipEditorGlobal;

ParamEditorGraphicsScene::ParamEditorGraphicsScene(QObject *parent) : TimeGraphicsScene(parent) {
}

void ParamEditorGraphicsScene::onViewResized(QSize size) {
    m_viewSize = size;
    updateSceneRect();
}

void ParamEditorGraphicsScene::updateSceneRect() {
    // 参数编辑器场景填充整个视图
    auto targetSceneWidth = sceneSize().width() * scaleX();
    auto targetRect = QRectF(0, 0, targetSceneWidth, m_viewSize.height());
    setSceneRect(targetRect);
    Log::d(CLASS_NAME, QString("Update scene rect: ") + qStrRectF(targetRect));
}