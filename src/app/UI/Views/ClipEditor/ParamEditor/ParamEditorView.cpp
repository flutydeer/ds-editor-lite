//
// Created by fluty on 24-8-21.
//

#include "ParamEditorView.h"

#include "ParamEditorGraphicsScene.h"
#include "ParamEditorGraphicsView.h"
#include "ParamEditorInfoArea.h"
#include "ParamEditorToolBarView.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include "Model/AppModel/SingingClip.h"
#include "UI/Utils/ParamNameUtils.h"

#include <QVBoxLayout>

ParamEditorView::ParamEditorView(QWidget *parent) : QWidget(parent) {
    auto scene = new ParamEditorGraphicsScene;

    auto infoArea = new ParamEditorInfoArea;
    infoArea->setFixedWidth(ClipEditorGlobal::pianoKeyboardWidth);
    m_graphicsView = new ParamEditorGraphicsView(scene, this);
    connect(m_graphicsView, &ParamEditorGraphicsView::sizeChanged, scene,
            &ParamEditorGraphicsScene::onViewResized);

    auto layout = new QHBoxLayout;
    layout->addWidget(infoArea);
    layout->addWidget(m_graphicsView);
    // layout->setContentsMargins(0, 0, 0, 6);
    layout->setContentsMargins(0, 0, 0, 0);

    auto toolBar = new ParamEditorToolBarView;

    auto mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(toolBar);
    mainLayout->addLayout(layout);
    setLayout(mainLayout);
    setMinimumHeight(128);

    connect(toolBar, &ParamEditorToolBarView::foregroundChanged, this,
            &ParamEditorView::onForegroundChanged);
    connect(toolBar, &ParamEditorToolBarView::backgroundChanged, this,
            &ParamEditorView::onBackgroundChanged);
}

void ParamEditorView::setDataContext(SingingClip *clip) const {
    m_graphicsView->setDataContext(clip);
}

ParamEditorGraphicsView *ParamEditorView::graphicsView() const {
    return m_graphicsView;
}

void ParamEditorView::onForegroundChanged(ParamInfo::Name name) const {
    qDebug() << "foreground changed" << paramNameUtils->nameFromType(name);
    m_graphicsView->setForeground(name);
}

void ParamEditorView::onBackgroundChanged(ParamInfo::Name name) const {
    qDebug() << "background changed" << paramNameUtils->nameFromType(name);
    m_graphicsView->setBackground(name);
}