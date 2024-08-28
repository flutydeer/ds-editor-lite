//
// Created by fluty on 24-8-21.
//

#include "ParamEditorView.h"

#include "ParamEditorGraphicsScene.h"
#include "ParamEditorGraphicsView.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QVBoxLayout>

ParamEditorView::ParamEditorView(QWidget *parent) : QWidget(parent) {
    auto scene = new ParamEditorGraphicsScene;
    m_graphicsView = new ParamEditorGraphicsView(scene, this);
    connect(m_graphicsView, &ParamEditorGraphicsView::sizeChanged, scene, &ParamEditorGraphicsScene::onViewResized);

    auto layout = new QHBoxLayout;
    layout->addSpacing(ClipEditorGlobal::pianoKeyboardWidth);
    layout->addWidget(m_graphicsView);
    layout->setContentsMargins(0, 0, 0, 6);
    setLayout(layout);
    setMinimumHeight(128);
}