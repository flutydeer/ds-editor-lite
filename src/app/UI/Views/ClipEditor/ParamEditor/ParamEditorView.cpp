//
// Created by fluty on 24-8-21.
//

#include "ParamEditorView.h"

#include "ParamEditorGraphicsScene.h"
#include "ParamEditorGraphicsView.h"
#include "ParamEditorInfoArea.h"
#include "ParamEditorToolBarView.h"
#include "SpeakerMixEditorView.h"
#include "SpeakerMixToolBarView.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Controller/PlaybackController.h"
#include "Utils/ParamUtils.h"

#include <QVBoxLayout>

ParamEditorView::ParamEditorView(QWidget *parent) : QWidget(parent) {
    const auto option = appOptions->general();
    const auto foregroundParam = option->defaultForegroundParam;
    const auto backgroundParam = option->defaultBackgroundParam;
    const auto foregroundProperties = paramUtils->getPropertiesByName(foregroundParam);
    const auto backgroundProperties = paramUtils->getPropertiesByName(backgroundParam);

    m_infoArea = new ParamEditorInfoArea;
    m_infoArea->setParamProperties(*foregroundProperties);
    m_infoArea->setFixedWidth(ClipEditorGlobal::pianoKeyboardWidth);

    const auto scene = new ParamEditorGraphicsScene;
    m_graphicsView =
        new ParamEditorGraphicsView(scene, *foregroundProperties, *backgroundProperties, this);
    connect(m_graphicsView, &ParamEditorGraphicsView::sizeChanged, scene,
            &ParamEditorGraphicsScene::onViewResized);

    const auto layout = new QHBoxLayout;
    layout->addWidget(m_infoArea);
    layout->addWidget(m_graphicsView);
    layout->setContentsMargins(0, 0, 0, 0);

    m_toolBar = new ParamEditorToolBarView;
    m_speakerMixToolBar = new SpeakerMixToolBarView;
    m_speakerMixToolBar->setVisible(false);

    const auto mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_toolBar);
    mainLayout->addWidget(m_speakerMixToolBar);
    mainLayout->addLayout(layout);
    setLayout(mainLayout);
    setMinimumHeight(128);

    connect(m_toolBar, &ParamEditorToolBarView::foregroundChanged, this,
            &ParamEditorView::onForegroundChanged);
    connect(m_toolBar, &ParamEditorToolBarView::backgroundChanged, this,
            &ParamEditorView::onBackgroundChanged);

    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::previousKeyframe, this,
            &ParamEditorView::onPreviousKeyframe);
    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::nextKeyframe, this,
            &ParamEditorView::onNextKeyframe);
}

void ParamEditorView::setDataContext(SingingClip *clip) const {
    m_graphicsView->setDataContext(clip);
}

ParamEditorGraphicsView *ParamEditorView::graphicsView() const {
    return m_graphicsView;
}

void ParamEditorView::onForegroundChanged(const ParamInfo::Name name) const {
    if (name == ParamInfo::Unknown) {
        qDebug() << "foreground changed to Speaker Mix";
        m_infoArea->clearParamProperties();
        m_graphicsView->setForeground(name, *paramUtils->getPropertiesByName(name));
        m_toolBar->setVisible(false);

        auto *mixView = m_graphicsView->speakerMixView();
        if (mixView) {
            QStringList names;
            QList<QColor> colors;
            for (const auto &speaker : mixView->speakers()) {
                names.append(speaker.name);
                colors.append(speaker.color);
            }
            m_speakerMixToolBar->setSpeakers(names, colors);
        }
        m_speakerMixToolBar->setVisible(true);
        return;
    }
    qDebug() << "foreground changed" << paramUtils->nameFromType(name);
    m_infoArea->setParamProperties(*paramUtils->getPropertiesByName(name));
    m_graphicsView->setForeground(name, *paramUtils->getPropertiesByName(name));
    m_toolBar->setVisible(true);
    m_speakerMixToolBar->setVisible(false);
}

void ParamEditorView::onBackgroundChanged(const ParamInfo::Name name) const {
    qDebug() << "background changed" << paramUtils->nameFromType(name);
    m_graphicsView->setBackground(name, *paramUtils->getPropertiesByName(name));
}

void ParamEditorView::onPreviousKeyframe() const {
    auto *mixView = m_graphicsView->speakerMixView();
    if (!mixView)
        return;

    const double currentTick = playbackController->position();
    const double prevTick = mixView->previousKeyframeTick(currentTick);
    if (prevTick >= 0) {
        m_graphicsView->setViewportCenterAtTick(prevTick);
        playbackController->setPosition(prevTick);
    }
}

void ParamEditorView::onNextKeyframe() const {
    auto *mixView = m_graphicsView->speakerMixView();
    if (!mixView)
        return;

    const double currentTick = playbackController->position();
    const double nextTick = mixView->nextKeyframeTick(currentTick);
    if (nextTick >= 0) {
        m_graphicsView->setViewportCenterAtTick(nextTick);
        playbackController->setPosition(nextTick);
    }
}
