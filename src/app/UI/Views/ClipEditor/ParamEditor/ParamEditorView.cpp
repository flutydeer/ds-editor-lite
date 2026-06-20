//
// Created by fluty on 24-8-21.
//

#include "ParamEditorView.h"

#include "ParamEditorGraphicsScene.h"
#include "ParamEditorGraphicsView.h"
#include "ParamEditorInfoArea.h"
#include "ParamEditorToolBarView.h"
#include "SpeakerMixEditorView.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include "Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Controller/PlaybackController.h"
#include "Modules/History/HistoryManager.h"
#include "Utils/ParamUtils.h"

#include <QVBoxLayout>

using namespace SpeakerMixModel;

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

    const auto mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_toolBar);
    mainLayout->addLayout(layout);
    setLayout(mainLayout);
    setMinimumHeight(128);

    connect(m_toolBar, &ParamEditorToolBarView::foregroundChanged, this,
            &ParamEditorView::onForegroundChanged);
    connect(m_toolBar, &ParamEditorToolBarView::backgroundChanged, this,
            &ParamEditorView::onBackgroundChanged);
    connect(m_toolBar, &ParamEditorToolBarView::previousKeyframe, this,
            &ParamEditorView::onPreviousKeyframe);
    connect(m_toolBar, &ParamEditorToolBarView::nextKeyframe, this,
            &ParamEditorView::onNextKeyframe);
    connect(m_toolBar, &ParamEditorToolBarView::dynamicMixToggled, this,
            &ParamEditorView::onDynamicMixToggled);

    auto *mixView = m_graphicsView->speakerMixView();
    connect(mixView, &SpeakerMixEditorView::speakerMixEdited, this,
            &ParamEditorView::onSpeakerMixEdited);
}

void ParamEditorView::setDataContext(SingingClip *clip) {
    if (m_clip)
        disconnect(m_clip, &SingingClip::speakerMixChanged, this,
                   &ParamEditorView::refreshSpeakerMixToolBar);
    m_clip = clip;
    m_graphicsView->setDataContext(clip);
    if (m_clip)
        connect(m_clip, &SingingClip::speakerMixChanged, this,
                &ParamEditorView::refreshSpeakerMixToolBar);
    refreshSpeakerMixToolBar();
}

ParamEditorGraphicsView *ParamEditorView::graphicsView() const {
    return m_graphicsView;
}

void ParamEditorView::onForegroundChanged(const ParamInfo::Name name) const {
    if (name == ParamInfo::Unknown) {
        qDebug() << "foreground changed to Speaker Mix";
        m_infoArea->clearParamProperties();
        m_graphicsView->setForeground(name, *paramUtils->getPropertiesByName(name));

        auto *mixView = m_graphicsView->speakerMixView();
        if (mixView && m_clip)
            mixView->setSpeakerMixData(m_clip->speakerMixData());
        refreshSpeakerMixToolBar();
        m_toolBar->setSpeakerMixMode(true);
        return;
    }
    qDebug() << "foreground changed" << paramUtils->nameFromType(name);
    m_infoArea->setParamProperties(*paramUtils->getPropertiesByName(name));
    m_graphicsView->setForeground(name, *paramUtils->getPropertiesByName(name));
    m_toolBar->setSpeakerMixMode(false);
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

void ParamEditorView::onSpeakerMixEdited(const SpeakerMixData &data) const {
    if (!m_clip)
        return;

    const auto normalized = normalizeSpeakerMixData(data);
    if (normalized == m_clip->speakerMixData())
        return;

    const auto actions = new SpeakerMixActions;
    actions->replaceSpeakerMix(normalized, m_clip);
    actions->execute();
    historyManager->record(actions);
}

void ParamEditorView::onDynamicMixToggled(const bool checked) const {
    if (!m_clip)
        return;

    auto data = m_clip->speakerMixData();
    if (data.sources.size() < 2)
        return;

    const int explicitWeightCount = data.sources.size() - 1;
    if (checked) {
        data.mode = SingerSourceMode::DynamicMix;
        if (data.dynamicKeyframes.isEmpty()) {
            if (data.fixedWeights.size() != explicitWeightCount)
                return;
            data.dynamicKeyframes.append({0, data.fixedWeights});
        }
    } else {
        data.mode = SingerSourceMode::FixedMix;
        if (data.fixedWeights.size() != explicitWeightCount) {
            if (data.dynamicKeyframes.isEmpty())
                return;
            data.fixedWeights = data.dynamicKeyframes.first().weights;
        }
    }

    onSpeakerMixEdited(data);
}

void ParamEditorView::refreshSpeakerMixToolBar() const {
    auto *mixView = m_graphicsView->speakerMixView();
    if (mixView && m_clip)
        mixView->setSpeakerMixData(m_clip->speakerMixData());

    QStringList names;
    QList<QColor> colors;
    if (mixView) {
        for (const auto &speaker : mixView->speakers()) {
            names.append(speaker.name);
            colors.append(speaker.color);
        }
    }
    m_toolBar->setSpeakers(names, colors);

    const auto data = m_clip ? normalizeSpeakerMixData(m_clip->speakerMixData()) : SpeakerMixData();
    const bool canUseFixed = !data.fixedWeights.isEmpty();
    const bool canUseDynamic = !data.dynamicKeyframes.isEmpty();
    const bool enabled =
        data.sources.size() >= 2 && data.mode != SingerSourceMode::Single &&
        (canUseFixed || canUseDynamic);
    m_toolBar->setDynamicMixEnabled(enabled);
    m_toolBar->setDynamicMixChecked(data.mode == SingerSourceMode::DynamicMix);
}
