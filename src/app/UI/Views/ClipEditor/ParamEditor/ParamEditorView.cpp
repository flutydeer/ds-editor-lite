//
// Created by fluty on 24-8-21.
//

#include "ParamEditorView.h"

#include "ParamEditorGraphicsScene.h"
#include "ParamEditorGraphicsView.h"
#include "ParamEditorInfoArea.h"
#include "ParamEditorToolBarView.h"
#include "SpeakerMixEditorView.h"
#include "UI/Controls/Button.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include "Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Controller/PlaybackController.h"
#include "Modules/History/HistoryManager.h"
#include "Utils/ParamUtils.h"
#include "UI/Dialogs/Base/MessageDialog.h"

#include <algorithm>
#include <QFontMetrics>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
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
    connect(m_graphicsView, &ParamEditorGraphicsView::sizeChanged, this,
            &ParamEditorView::updateSpeakerMixEmptyStateGeometry);

    connect(m_graphicsView->speakerMixView(), &SpeakerMixEditorView::speakerColorsChanged, this,
            &ParamEditorView::refreshSpeakerMixToolBar);

    const auto layout = new QHBoxLayout;
    layout->addWidget(m_infoArea);
    layout->addWidget(m_graphicsView);
    layout->setContentsMargins(0, 0, 0, 0);

    m_speakerMixEmptyState = new QWidget(m_graphicsView->viewport());
    m_speakerMixEmptyState->setObjectName("speakerMixEmptyState");
    m_speakerMixEmptyState->setAttribute(Qt::WA_StyledBackground);
    m_speakerMixEmptyState->setVisible(false);

    m_speakerMixEmptyTitle = new QLabel;
    m_speakerMixEmptyTitle->setAlignment(Qt::AlignCenter);
    m_speakerMixEmptyMessage = new QLabel;
    m_speakerMixEmptyMessage->setAlignment(Qt::AlignCenter);
    m_speakerMixEmptyMessage->setWordWrap(true);
    m_enableDynamicMixButton = new Button(tr("Enable Dynamic Mix"));
    m_enableDynamicMixButton->setObjectName("btnEnableDynamicMix");

    m_speakerMixEmptyLayout = new QVBoxLayout;
    m_speakerMixEmptyLayout->addStretch();
    m_speakerMixEmptyLayout->addWidget(m_speakerMixEmptyTitle, 0, Qt::AlignCenter);
    m_speakerMixEmptyLayout->addWidget(m_speakerMixEmptyMessage, 0, Qt::AlignCenter);
    m_speakerMixEmptyLayout->addWidget(m_enableDynamicMixButton, 0, Qt::AlignCenter);
    m_speakerMixEmptyLayout->addStretch();
    m_speakerMixEmptyLayout->setContentsMargins(48, 12, 48, 12);
    m_speakerMixEmptyLayout->setSpacing(8);
    m_speakerMixEmptyState->setLayout(m_speakerMixEmptyLayout);

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
    connect(m_toolBar, &ParamEditorToolBarView::bypassDynamicMix, this,
            &ParamEditorView::onBypassDynamicMix);
    connect(m_toolBar, &ParamEditorToolBarView::resumeDynamicMix, this,
            &ParamEditorView::onResumeDynamicMix);
    connect(m_toolBar, &ParamEditorToolBarView::stopDynamicMix, this,
            &ParamEditorView::onStopDynamicMix);
    connect(m_enableDynamicMixButton, &Button::clicked, this, &ParamEditorView::onEnableDynamicMix);

    auto *mixView = m_graphicsView->speakerMixView();
    connect(mixView, &SpeakerMixEditorView::speakerMixEdited, this,
            &ParamEditorView::onSpeakerMixEdited);
}

void ParamEditorView::setDataContext(SingingClip *clip) {
    if (m_clip)
        disconnect(m_clip, &SingingClip::voiceContextChanged, this, nullptr);
    m_clip = clip;
    m_graphicsView->setDataContext(clip);
    if (m_clip)
        connect(m_clip, &SingingClip::voiceContextChanged, this,
                [this](const VoiceContextChange &) { refreshSpeakerMixToolBar(); });
    refreshSpeakerMixToolBar();
}

ParamEditorGraphicsView *ParamEditorView::graphicsView() const {
    return m_graphicsView;
}

void ParamEditorView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        refreshSpeakerMixToolBar();
}

void ParamEditorView::onForegroundChanged(const ParamInfo::Name name) {
    if (name == ParamInfo::SpeakerMix) {
        qDebug() << "foreground changed to Speaker Mix";
        m_infoArea->clearParamProperties();
        m_graphicsView->setForeground(name, *paramUtils->getPropertiesByName(name));

        auto *mixView = m_graphicsView->speakerMixView();
        if (mixView && m_clip) {
            mixView->setReferenceSpeakers(m_clip->singerInfo().speakers());
            mixView->setSpeakerMixData(m_clip->speakerMixData());
        }
        refreshSpeakerMixToolBar();
        m_toolBar->setSpeakerMixMode(true);
        return;
    }
    qDebug() << "foreground changed" << paramUtils->nameFromType(name);
    m_infoArea->setParamProperties(*paramUtils->getPropertiesByName(name));
    m_graphicsView->setForeground(name, *paramUtils->getPropertiesByName(name));
    m_toolBar->setSpeakerMixMode(false);
    hideSpeakerMixEmptyState();
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

void ParamEditorView::onEnableDynamicMix() {
    if (!m_clip)
        return;

    const auto data = dataWithDynamicEnabled(m_clip->speakerMixData());
    if (data.mode != SingerSourceMode::DynamicMix)
        return;

    const auto actions = new SpeakerMixActions;
    actions->enableClipDynamicSpeakerMix(m_clip->singerInfo(), m_clip->speakerInfo(), data, m_clip);
    actions->execute();
    historyManager->record(actions);
}

void ParamEditorView::onBypassDynamicMix() const {
    if (!m_clip || m_clip->usesTrackVoiceContext())
        return;

    auto data = m_clip->speakerMixData();
    if (!isDynamicMixActive(data))
        return;

    data.dynamicBypassed = true;
    onSpeakerMixEdited(data);
}

void ParamEditorView::onResumeDynamicMix() const {
    if (!m_clip || m_clip->usesTrackVoiceContext())
        return;

    auto data = m_clip->speakerMixData();
    if (!isDynamicMixBypassed(data))
        return;

    data.dynamicBypassed = false;
    onSpeakerMixEdited(data);
}

void ParamEditorView::onStopDynamicMix() {
    if (!m_clip || m_clip->usesTrackVoiceContext())
        return;

    auto data = m_clip->speakerMixData();
    if (data.dynamicKeyframes.isEmpty())
        return;

    constexpr int keepDynamicMixButtonId = 0;
    constexpr int stopDynamicMixButtonId = 1;
    MessageDialog dialog(
        tr("Stop using dynamic speaker mix?"),
        tr("This will delete all dynamic mix keyframes and return this clip to fixed mix."), this);
    dialog.setTitle(tr("Stop using dynamic speaker mix?"));
    dialog.addAccentButton(tr("停止使用动态混合"), stopDynamicMixButtonId);
    dialog.addButton(tr("取消"), keepDynamicMixButtonId);
    if (dialog.exec() != stopDynamicMixButtonId) {
        return;
    }

    data = dataWithDynamicStopped(data);
    onSpeakerMixEdited(data);
}

void ParamEditorView::refreshSpeakerMixToolBar() {
    auto *mixView = m_graphicsView->speakerMixView();
    if (mixView && m_clip) {
        mixView->setReferenceSpeakers(m_clip->singerInfo().speakers());
        mixView->setSpeakerMixData(m_clip->speakerMixData());
    }

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
    SpeakerMixDynamicUiState state = SpeakerMixDynamicUiState::Unavailable;
    if (isDynamicMixActive(data)) {
        state = SpeakerMixDynamicUiState::Active;
    } else if (isDynamicMixBypassed(data)) {
        state = SpeakerMixDynamicUiState::Bypassed;
    } else if (hasFixedMixBase(data)) {
        state = SpeakerMixDynamicUiState::NotEnabled;
    }
    m_toolBar->setSpeakerMixDynamicState(state);
    refreshSpeakerMixEmptyState(data);
}

void ParamEditorView::refreshSpeakerMixEmptyState(const SpeakerMixData &data) {
    const auto *mixView = m_graphicsView->speakerMixView();
    if (!mixView || !mixView->isVisible()) {
        hideSpeakerMixEmptyState();
        return;
    }

    if (!data.dynamicKeyframes.isEmpty()) {
        hideSpeakerMixEmptyState();
        return;
    }

    if (!hasFixedMixBase(data)) {
        setSpeakerMixEmptyState(tr("Dynamic mix is unavailable"),
                                tr("Choose a fixed speaker mix preset with at least two speakers "
                                   "before enabling dynamic mix."),
                                tr("Enable Dynamic Mix"), false);
        return;
    }

    if (m_clip && m_clip->usesTrackVoiceContext()) {
        setSpeakerMixEmptyState(tr("Enable clip dynamic mix?"),
                                tr("This clip is following the track. Enabling dynamic mix will "
                                   "copy the current track speaker mix to this clip and stop "
                                   "following the track."),
                                tr("Copy and Enable Dynamic Mix"), true);
        return;
    }

    setSpeakerMixEmptyState(tr("Enable Dynamic Mix"),
                            tr("Create the first keyframe from the current fixed speaker mix."),
                            tr("Enable Dynamic Mix"), true);
}

void ParamEditorView::setSpeakerMixEmptyState(const QString &title, const QString &message,
                                              const QString &buttonText, const bool buttonEnabled) {
    m_speakerMixEmptyTitle->setText(title);
    m_speakerMixEmptyMessageText = message;
    m_speakerMixEmptyMessage->setText(message);
    m_enableDynamicMixButton->setText(buttonText);
    m_enableDynamicMixButton->setEnabled(buttonEnabled);
    m_speakerMixEmptyMessage->setToolTip({});
    m_enableDynamicMixButton->setToolTip({});
    m_speakerMixEmptyState->setToolTip({});
    updateSpeakerMixEmptyStateGeometry();
    m_speakerMixEmptyState->show();
    m_speakerMixEmptyState->raise();
}

void ParamEditorView::hideSpeakerMixEmptyState() {
    if (m_speakerMixEmptyState)
        m_speakerMixEmptyState->hide();
}

void ParamEditorView::updateSpeakerMixEmptyStateGeometry() {
    if (!m_speakerMixEmptyState)
        return;

    const auto viewportRect = m_graphicsView->viewport()->rect();
    m_speakerMixEmptyState->setGeometry(viewportRect);

    const int height = viewportRect.height();
    const bool compact = height < 180;
    const bool minimal = height < 120;
    const int horizontalMargin = minimal ? 12 : compact ? 24 : 48;
    const int verticalMargin = minimal ? 4 : compact ? 8 : 12;
    const int contentWidth =
        std::max(1, std::min(840, viewportRect.width() - horizontalMargin * 2));

    m_speakerMixEmptyTitle->setFixedWidth(contentWidth);
    m_speakerMixEmptyMessage->setFixedWidth(contentWidth);
    m_speakerMixEmptyMessage->setVisible(true);

    if (compact) {
        m_speakerMixEmptyMessage->setWordWrap(false);
        m_speakerMixEmptyMessage->setText(m_speakerMixEmptyMessage->fontMetrics().elidedText(
            m_speakerMixEmptyMessageText, Qt::ElideRight, contentWidth));
    } else {
        m_speakerMixEmptyMessage->setWordWrap(true);
        m_speakerMixEmptyMessage->setText(m_speakerMixEmptyMessageText);
    }

    if (m_speakerMixEmptyLayout) {
        m_speakerMixEmptyLayout->setContentsMargins(horizontalMargin, verticalMargin,
                                                    horizontalMargin, verticalMargin);
        m_speakerMixEmptyLayout->setSpacing(minimal ? 4 : compact ? 6 : 8);
    }
}

bool ParamEditorView::hasFixedMixBase(const SpeakerMixData &data) {
    const auto normalized = normalizeSpeakerMixData(data);
    return normalized.mode == SingerSourceMode::FixedMix && normalized.sources.size() >= 2 &&
           normalized.fixedWeights.size() == normalized.sources.size() - 1;
}

SpeakerMixData ParamEditorView::dataWithDynamicEnabled(const SpeakerMixData &data) {
    auto result = normalizeSpeakerMixData(data);
    if (!hasFixedMixBase(result) && result.dynamicKeyframes.isEmpty())
        return {};

    result.mode = SingerSourceMode::DynamicMix;
    result.dynamicBypassed = false;
    if (result.dynamicKeyframes.isEmpty())
        result.dynamicKeyframes.append({0, result.fixedWeights});
    return normalizeSpeakerMixData(result);
}

SpeakerMixData ParamEditorView::dataWithDynamicStopped(const SpeakerMixData &data) {
    auto result = normalizeSpeakerMixData(data);
    if (result.dynamicKeyframes.isEmpty())
        return result;

    const int explicitWeightCount = result.sources.size() - 1;
    if (result.fixedWeights.size() != explicitWeightCount)
        result.fixedWeights = result.dynamicKeyframes.first().weights;
    result.dynamicKeyframes.clear();
    result.mode = SingerSourceMode::FixedMix;
    result.dynamicBypassed = false;
    return normalizeSpeakerMixData(result);
}
