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
#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/ParamNameUtils.h"

#include <QVBoxLayout>

ParamEditorView::ParamEditorView(QWidget *parent) : QWidget(parent) {
    auto option = appOptions->general();
    auto foregroundParam = option->defaultForegroundParam;
    auto backgroundParam = option->defaultBackgroundParam;
    auto foregroundProperties = getPropertiesByName(foregroundParam);
    auto backgroundProperties = getPropertiesByName(backgroundParam);

    m_infoArea = new ParamEditorInfoArea;
    m_infoArea->setParamProperties(*foregroundProperties);
    m_infoArea->setFixedWidth(ClipEditorGlobal::pianoKeyboardWidth);

    auto scene = new ParamEditorGraphicsScene;
    m_graphicsView =
        new ParamEditorGraphicsView(scene, *foregroundProperties, *backgroundProperties, this);
    connect(m_graphicsView, &ParamEditorGraphicsView::sizeChanged, scene,
            &ParamEditorGraphicsScene::onViewResized);

    auto layout = new QHBoxLayout;
    layout->addWidget(m_infoArea);
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
    m_infoArea->setParamProperties(*getPropertiesByName(name));
    m_graphicsView->setForeground(name, *getPropertiesByName(name));
}

void ParamEditorView::onBackgroundChanged(ParamInfo::Name name) const {
    qDebug() << "background changed" << paramNameUtils->nameFromType(name);
    m_graphicsView->setBackground(name, *getPropertiesByName(name));
}

const ParamProperties *ParamEditorView::getPropertiesByName(ParamInfo::Name name) const {
    switch (name) {
        case ParamInfo::Pitch:
            return &pitchProperties;
        case ParamInfo::Expressiveness:
            return &exprProperties;
        case ParamInfo::Energy:
        case ParamInfo::Breathiness:
        case ParamInfo::Voicing:
            return &decibelProperties;
        case ParamInfo::Tension:
            return &tensionProperties;
        case ParamInfo::Gender:
            return &genderProperties;
        case ParamInfo::Velocity:
            return &velocityProperties;
        case ParamInfo::Unknown:
            return &defaultProperties;
    }
    return &defaultProperties;
}