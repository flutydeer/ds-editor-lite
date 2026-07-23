//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorView.h"

#include "Controller/ClipController.h"
#include "Controller/EditorViewController.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/Track.h"
#include "Model/AppStatus/AppStatus.h"
#include "ParamEditor/ParamEditorView.h"
#include "PianoRoll/NoteView.h"
#include "PianoRoll/PhonemeView.h"
#include "PianoRoll/PianoRollGraphicsView.h"
#include "PianoRoll/PianoRollView.h"
#include "ToolBar/ClipEditorToolBarView.h"

#include <QLabel>
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>

#include <cmath>

QString ClipEditorView::tabId() const {
    return "ClipEditor";
}

QString ClipEditorView::tabName() const {
    return tr("Edit");
}

AppGlobal::PanelType ClipEditorView::panelType() const {
    return AppGlobal::PanelType::ClipEditor;
}

QWidget *ClipEditorView::toolBar() {
    return m_toolbarView;
}

QWidget *ClipEditorView::content() {
    return this;
}

bool ClipEditorView::isToolBarVisible() const {
    return m_hasActiveClip;
}

ClipEditorView::ClipEditorView(QWidget *parent) : TabPanelPage(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("ClipEditorView");

    m_toolbarView = new ClipEditorToolBarView;
    m_toolbarView->setVisible(false);

    m_pianoRollEditorView = new PianoRollEditorView;
    m_pianoRollEditorView->setVisible(false);

    m_placeholderLabel = new QLabel(this);
    m_placeholderLabel->setObjectName("clipEditorPlaceholder");
    m_placeholderLabel->setText(tr("Please select a singing clip to edit"));
    m_placeholderLabel->setAlignment(Qt::AlignCenter);

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_placeholderLabel);
    mainLayout->addWidget(m_pianoRollEditorView);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({1, 0, 1, 1});
    setLayout(mainLayout);

    installEventFilter(this);

    connect(m_toolbarView, &ClipEditorToolBarView::editModeChanged,
            m_pianoRollEditorView->pianoRollView(), &PianoRollView::onEditModeChanged);

    connect(appModel, &AppModel::modelChanged, this, &ClipEditorView::onModelChanged);
    connect(appStatus, &AppStatus::activeClipIdChanged, this, &ClipEditorView::onActiveClipChanged);
}

PianoRollViewState ClipEditorView::viewState() const {
    const auto graphicsView = m_pianoRollEditorView->pianoRollView()->graphicsView();
    return {
        .centerTick = (graphicsView->startTick() + graphicsView->endTick()) / 2,
        .centerKeyIndex = graphicsView->centerKeyIndex(),
        .horizontalScale = graphicsView->scaleX(),
        .verticalScale = graphicsView->scaleY(),
        .editMode = m_toolbarView->editMode(),
    };
}

bool ClipEditorView::supportsEditMode(const EditorViewGlobal::PianoRollEditMode mode) const {
    return m_toolbarView->supportsEditMode(mode);
}

bool ClipEditorView::centerAt(const double tick, const double keyIndex) const {
    if (!std::isfinite(tick) || !std::isfinite(keyIndex))
        return false;
    const auto graphicsView = m_pianoRollEditorView->pianoRollView()->graphicsView();
    graphicsView->stopViewportAnimations();
    graphicsView->setViewportCenterAt(tick, keyIndex, false);
    return true;
}

bool ClipEditorView::setViewScale(const double horizontalScale, const double verticalScale) const {
    const auto previousState = viewState();
    const auto graphicsView = m_pianoRollEditorView->pianoRollView()->graphicsView();
    if (!graphicsView->setViewportScale(horizontalScale, verticalScale))
        return false;
    return centerAt(previousState.centerTick, previousState.centerKeyIndex);
}

bool ClipEditorView::setEditMode(const EditorViewGlobal::PianoRollEditMode mode) {
    return m_toolbarView->setEditMode(mode);
}

HistoryFocusVisibility ClipEditorView::focusVisibility(const HistoryFocus &focus) const {
    return m_pianoRollEditorView->pianoRollView()->graphicsView()->focusVisibility(focus);
}

bool ClipEditorView::revealFocus(const HistoryFocus &focus) const {
    return m_pianoRollEditorView->pianoRollView()->graphicsView()->revealFocus(focus);
}

void ClipEditorView::refreshActiveClipTrackPresentation() {
    Track *trackRef = nullptr;
    appModel->findClipById(appStatus->activeClipId, trackRef);
    if (!trackRef)
        return;

    disconnect(m_trackColorConnection);
    applyTrackColor(trackRef->colorIndex());
    m_trackColorConnection = connect(trackRef, &Track::propertyChanged, this,
                                     [this, trackRef] { applyTrackColor(trackRef->colorIndex()); });
}

void ClipEditorView::previewActiveClipTrackColor(const int colorIndex) const {
    applyTrackColor(colorIndex);
}

void ClipEditorView::onModelChanged() {
    m_toolbarView->setDataContext(nullptr);
    reset();
}

void ClipEditorView::onActiveClipChanged(const int clipId) {
    Track *trackRef = nullptr;
    const auto clip = appModel->findClipById(clipId, trackRef);
    m_toolbarView->setDataContext(clip);
    clipController->setClip(clip);

    disconnect(m_trackColorConnection);

    if (trackRef) {
        applyTrackColor(trackRef->colorIndex());
        m_trackColorConnection = connect(trackRef, &Track::propertyChanged, this, [this, trackRef] {
            applyTrackColor(trackRef->colorIndex());
        });
    }

    bool hadActiveClip = m_hasActiveClip;
    if (clip == nullptr) {
        moveToNullClipState();
    } else if (clip->clipType() == Clip::Singing) {
        moveToSingingClipState(dynamic_cast<SingingClip *>(clip));
    } else if (clip->clipType() == Clip::Audio) {
        moveToAudioClipState(nullptr);
    }

    if (hadActiveClip != m_hasActiveClip) {
        emit toolBarVisibilityChanged();
    }
}

bool ClipEditorView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QMouseEvent::MouseButtonPress)
        editorViewController->setActivePanel(AppGlobal::ClipEditor);
    return QWidget::eventFilter(watched, event);
}

void ClipEditorView::moveToSingingClipState(SingingClip *clip) const {
    m_hasActiveClip = true;
    m_placeholderLabel->setVisible(false);
    m_pianoRollEditorView->setDataContext(clip);
    m_pianoRollEditorView->setVisible(true);
    m_pianoRollEditorView->pianoRollView()->onEditModeChanged(m_toolbarView->editMode());
}

void ClipEditorView::moveToAudioClipState(const AudioClip *clip) const {
    Q_UNUSED(clip);
    m_hasActiveClip = true;
    m_pianoRollEditorView->setVisible(false);
    m_placeholderLabel->setVisible(true);
    m_pianoRollEditorView->setDataContext(nullptr);
}

void ClipEditorView::moveToNullClipState() const {
    m_hasActiveClip = false;
    m_pianoRollEditorView->setVisible(false);
    m_placeholderLabel->setVisible(true);
    m_pianoRollEditorView->setDataContext(nullptr);
}

void ClipEditorView::reset() {
    onActiveClipChanged(-1);
}

void ClipEditorView::applyTrackColor(const int colorIndex) const {
    NoteView::setTrackColorIndex(colorIndex);
    m_pianoRollEditorView->pianoRollView()->setTrackColorIndex(colorIndex);
    m_pianoRollEditorView->pianoRollView()->update();
    m_pianoRollEditorView->paramEditorView()->update();
}

void ClipEditorView::changeEvent(QEvent *event) {
    TabPanelPage::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        m_placeholderLabel->setText(tr("Please select a singing clip to edit"));
}
