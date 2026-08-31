#include "ParamEditorView.h"

#include "ParamEditorGraphicsScene.h"
#include "ParamEditorGraphicsView.h"
#include "ParamEditorInfoArea.h"
#include "ParamEditorToolBarView.h"
#include "SpeakerMixEditorView.h"
#include <lite/GUI/Controls/Button.h>
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/UiLanguageManager.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/EditorViewController.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppOptions/AppOptions.h"
#include "Controller/PlaybackController.h"
#include "Model/Utils/ParamUtils.h"
#include "UI/Dialogs/Base/MessageDialog.h"

#include <algorithm>
#include <QFontMetrics>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

using namespace SpeakerMixModel;

namespace {
    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime) {
        return {.expected = runtime.documentVersion(),
                .source = Automation::InvocationSource::TrustedGui};
    }
}

ParamEditorView::ParamEditorView(QWidget *parent) : QWidget(parent) {
    editorViewController->registerInteractionArea(this, AppGlobal::ClipEditor,
                                                  EditorInteraction::Target::Parameters);
    const auto option = appOptions->general();
    const auto foregroundParam = option->defaultForegroundParam;
    const auto backgroundParam = option->defaultBackgroundParam;
    m_foregroundParam = foregroundParam;
    m_backgroundParam = backgroundParam;
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
            &ParamEditorView::updateEmptyStateGeometry);

    connect(m_graphicsView->speakerMixView(), &SpeakerMixEditorView::speakerColorsChanged, this,
            &ParamEditorView::refreshSpeakerMixToolBar);
    // Refresh the speaker mix toolbar after the LanguageChange event so the
    // editor view has already rebuilt its display names for the new language.
    if (const auto *langMgr = UiLanguageManager::instance()) {
        connect(langMgr, &UiLanguageManager::languageChanged, this,
                &ParamEditorView::refreshSpeakerMixToolBar);
    }

    const auto layout = new QHBoxLayout;
    layout->addWidget(m_infoArea);
    layout->addWidget(m_graphicsView);
    layout->setContentsMargins(0, 0, 0, 0);

    m_emptyState = new QWidget(m_graphicsView->viewport());
    m_emptyState->setObjectName("speakerMixEmptyState");
    m_emptyState->setAttribute(Qt::WA_NoMousePropagation);
    m_emptyState->setAttribute(Qt::WA_StyledBackground);
    m_emptyState->setVisible(false);

    m_emptyStateTitle = new QLabel;
    m_emptyStateTitle->setAlignment(Qt::AlignCenter);
    m_emptyStateMessage = new QLabel;
    m_emptyStateMessage->setAlignment(Qt::AlignCenter);
    m_emptyStateMessage->setWordWrap(true);
    m_emptyStateActionButton = new Button;

    m_emptyStateLayout = new QVBoxLayout;
    m_emptyStateLayout->addStretch();
    m_emptyStateLayout->addWidget(m_emptyStateTitle, 0, Qt::AlignCenter);
    m_emptyStateLayout->addWidget(m_emptyStateMessage, 0, Qt::AlignCenter);
    m_emptyStateLayout->addWidget(m_emptyStateActionButton, 0, Qt::AlignCenter);
    m_emptyStateLayout->addStretch();
    m_emptyStateLayout->setContentsMargins(48, 12, 48, 12);
    m_emptyStateLayout->setSpacing(8);
    m_emptyState->setLayout(m_emptyStateLayout);

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
    connect(m_toolBar, &ParamEditorToolBarView::editModeChanged, m_graphicsView,
            &ParamEditorGraphicsView::setEditMode);
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
    connect(m_emptyStateActionButton, &Button::clicked, this, &ParamEditorView::onEmptyStateAction);
    connect(
        editorViewController, &EditorViewController::editCommandRequested, this,
        [this](const EditorInteraction::Target target, const EditorInteraction::Command command) {
            if (target == EditorInteraction::Target::Parameters)
                executeEditCommand(command);
        });

    auto *mixView = m_graphicsView->speakerMixView();
    connect(mixView, &SpeakerMixEditorView::speakerMixEdited, this,
            &ParamEditorView::onSpeakerMixEdited);
    connect(appModel, &AppModel::modelChanged, this, [this] {
        m_unsupportedParameterPromptState.resetForProject();
        hideEmptyState();
    });
}

ParamEditorView::~ParamEditorView() {
    editorViewController->unregisterInteractionArea(this);
}

void ParamEditorView::executeEditCommand(const EditorInteraction::Command command) const {
    if (command == EditorInteraction::Command::DeleteSelection)
        m_graphicsView->deleteSelection();
}

void ParamEditorView::setDataContext(SingingClip *clip) {
    if (m_clip) {
        disconnect(m_clip, &SingingClip::voiceContextChanged, this, nullptr);
        disconnect(m_clip, &SingingClip::paramChanged, this, nullptr);
    }
    m_clip = clip;
    m_graphicsView->setDataContext(clip);
    if (m_clip) {
        connect(m_clip, &SingingClip::voiceContextChanged, this,
                [this](const VoiceContextChange &) {
                    refreshSpeakerMixToolBar();
                    refreshParameterSupportState();
                });
        connect(m_clip, &SingingClip::paramChanged, this,
                [this](const ParamInfo::Name name, const Param::Type type) {
                    if (name == m_foregroundParam && type == Param::Original)
                        refreshParameterSupportState();
                });
    }
    refreshSpeakerMixToolBar();
    refreshParameterSupportState();
}

ParamEditorGraphicsView *ParamEditorView::graphicsView() const {
    return m_graphicsView;
}

void ParamEditorView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        refreshParameterSupportState();
}

void ParamEditorView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    editorViewController->syncEditTargetVisibility(
        EditorInteraction::Target::Parameters, event->size().height() > 0, AppGlobal::ClipEditor);
}

void ParamEditorView::onForegroundChanged(const ParamInfo::Name name) {
    m_foregroundParam = name;
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
        refreshParameterSupportState();
        return;
    }
    qDebug() << "foreground changed" << paramUtils->nameFromType(name);
    m_infoArea->setParamProperties(*paramUtils->getPropertiesByName(name));
    m_graphicsView->setForeground(name, *paramUtils->getPropertiesByName(name));
    refreshParameterSupportState();
}

void ParamEditorView::onBackgroundChanged(const ParamInfo::Name name) {
    m_backgroundParam = name;
    qDebug() << "background changed"
             << (name == ParamInfo::Unknown ? QStringLiteral("(None)")
                                            : paramUtils->nameFromType(name));
    m_graphicsView->setBackground(name, *paramUtils->getPropertiesByName(name));
    refreshParameterSupportState();
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

    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
        runtime->parameters().replaceClipSpeakerMix(commandContext(*runtime),
                                                    Automation::ClipId(m_clip->id()), normalized);
}

void ParamEditorView::onEmptyStateAction() {
    const auto action = m_emptyStateAction;
    const auto parameter = m_emptyStateParameter;
    if (action == EmptyStateAction::EditUnsupportedParameter && parameter != ParamInfo::Unknown)
        m_unsupportedParameterPromptState.acknowledge(parameter);
    hideEmptyState();
    if (action == EmptyStateAction::EditUnsupportedParameter)
        refreshParameterSupportState();
    else if (action == EmptyStateAction::EnableDynamicMix)
        onEnableDynamicMix();
}

void ParamEditorView::onEnableDynamicMix() {
    if (!m_clip)
        return;

    const auto data = dataWithDynamicEnabled(m_clip->speakerMixData());
    if (data.mode != SingerSourceMode::DynamicMix)
        return;

    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
        runtime->parameters().enableClipDynamicSpeakerMix(
            commandContext(*runtime), Automation::ClipId(m_clip->id()), m_clip->singerInfo(),
            m_clip->speakerInfo(), data);
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
        if (m_emptyStateKind == EmptyStateKind::SpeakerMix)
            hideEmptyState();
        return;
    }

    if (!data.dynamicKeyframes.isEmpty()) {
        hideEmptyState();
        return;
    }

    if (!hasFixedMixBase(data)) {
        setEmptyState(EmptyStateKind::SpeakerMix, tr("Dynamic mix is unavailable"),
                      tr("Choose a fixed speaker mix preset with at least two speakers before "
                         "enabling dynamic mix."),
                      std::nullopt);
        return;
    }

    if (m_clip && m_clip->usesTrackVoiceContext()) {
        setEmptyState(EmptyStateKind::SpeakerMix, tr("Enable clip dynamic mix?"),
                      tr("This clip is following the track. Enabling dynamic mix will copy the "
                         "current track speaker mix to this clip and stop following the track."),
                      tr("Copy and Enable Dynamic Mix"), EmptyStateAction::EnableDynamicMix);
        return;
    }

    setEmptyState(EmptyStateKind::SpeakerMix, tr("Enable Dynamic Mix"),
                  tr("Create the first keyframe from the current fixed speaker mix."),
                  tr("Enable Dynamic Mix"), EmptyStateAction::EnableDynamicMix);
}

void ParamEditorView::refreshParameterSupportState() {
    const auto isSupported = [this](const ParamInfo::Name name) {
        return !m_clip || paramUtils->isSupportedBySinger(name, m_clip->singerInfo());
    };
    const bool foregroundSupported = isSupported(m_foregroundParam);
    const bool backgroundSupported = isSupported(m_backgroundParam);
    m_graphicsView->setForegroundBaseCurveVisible(foregroundSupported);
    m_graphicsView->setBackgroundBaseCurveVisible(backgroundSupported);

    const auto *foreground = m_clip && ParamInfo::hasOriginalParam(m_foregroundParam)
                                 ? m_clip->params.getParamByName(m_foregroundParam)
                                 : nullptr;
    const bool bakeEnabled = foregroundSupported && m_clip && !m_clip->singerInfo().isEmpty() &&
                             foreground && !foreground->curves(Param::Original).isEmpty();
    m_toolBar->setBakeEnabled(bakeEnabled);
    m_toolBar->setTransformEnabled(foregroundSupported && m_clip &&
                                   ParamInfo::supportsCurveTransform(m_foregroundParam));

    if (m_foregroundParam == ParamInfo::SpeakerMix)
        return;

    if (!m_unsupportedParameterPromptState.shouldPrompt(m_foregroundParam, foregroundSupported)) {
        hideEmptyState();
        return;
    }

    const auto showUnsupportedState = [this] {
        setEmptyState(EmptyStateKind::UnsupportedParameter, {},
                      tr("The selected singer does not support this parameter."), tr("Edit Anyway"),
                      EmptyStateAction::EditUnsupportedParameter, m_foregroundParam);
    };
    showUnsupportedState();
}

void ParamEditorView::setEmptyState(const EmptyStateKind kind, const QString &title,
                                    const QString &message,
                                    const std::optional<QString> &actionText,
                                    const EmptyStateAction action,
                                    const ParamInfo::Name parameter) {
    m_emptyStateKind = kind;
    m_emptyStateAction = actionText ? action : EmptyStateAction::None;
    m_emptyStateParameter = parameter;
    m_emptyStateTitle->setText(title);
    m_emptyStateTitle->setVisible(!title.isEmpty());
    m_emptyStateMessageText = message;
    m_emptyStateMessage->setText(message);
    if (actionText)
        m_emptyStateActionButton->setText(*actionText);
    m_emptyStateActionButton->setVisible(actionText.has_value());
    m_emptyStateMessage->setToolTip({});
    m_emptyStateActionButton->setToolTip({});
    m_emptyState->setToolTip({});
    updateEmptyStateGeometry();
    m_emptyState->show();
    m_emptyState->raise();
}

void ParamEditorView::hideEmptyState() {
    m_emptyStateKind = EmptyStateKind::None;
    m_emptyStateAction = EmptyStateAction::None;
    m_emptyStateParameter = ParamInfo::Unknown;
    if (m_emptyState)
        m_emptyState->hide();
}

void ParamEditorView::updateEmptyStateGeometry() {
    if (!m_emptyState)
        return;

    const auto viewportRect = m_graphicsView->viewport()->rect();
    m_emptyState->setGeometry(viewportRect);

    const int height = viewportRect.height();
    const bool compact = height < 180;
    const bool minimal = height < 120;
    const int horizontalMargin = minimal ? 12 : compact ? 24 : 48;
    const int verticalMargin = minimal ? 4 : compact ? 8 : 12;
    const int contentWidth =
        std::max(1, std::min(840, viewportRect.width() - horizontalMargin * 2));

    m_emptyStateTitle->setFixedWidth(contentWidth);
    m_emptyStateMessage->setFixedWidth(contentWidth);
    m_emptyStateMessage->setVisible(true);

    if (compact) {
        m_emptyStateMessage->setWordWrap(false);
        m_emptyStateMessage->setText(m_emptyStateMessage->fontMetrics().elidedText(
            m_emptyStateMessageText, Qt::ElideRight, contentWidth));
    } else {
        m_emptyStateMessage->setWordWrap(true);
        m_emptyStateMessage->setText(m_emptyStateMessageText);
    }

    if (m_emptyStateLayout) {
        m_emptyStateLayout->setContentsMargins(horizontalMargin, verticalMargin, horizontalMargin,
                                               verticalMargin);
        m_emptyStateLayout->setSpacing(minimal ? 4 : compact ? 6 : 8);
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
