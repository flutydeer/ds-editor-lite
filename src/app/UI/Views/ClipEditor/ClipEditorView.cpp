//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorView.h"

#include "Controller/AppController.h"
#include "Controller/ClipController.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/Track.h"
#include "Model/AppStatus/AppStatus.h"
#include "PianoRoll/NoteView.h"
#include "PianoRoll/PhonemeView.h"
#include "PianoRoll/PianoRollGraphicsView.h"
#include "PianoRoll/PianoRollView.h"
#include "ToolBar/ClipEditorToolBarView.h"

#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

QString ClipEditorView::tabId() const {
    return "ClipEditor";
}

QString ClipEditorView::tabName() const {
    return "编辑";
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

    const auto mainLayout = new QVBoxLayout;
    // mainLayout->addWidget(m_toolbarView);
    mainLayout->addWidget(m_pianoRollEditorView);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({1, 0, 1, 1});
    setLayout(mainLayout);

    clipController->setView(this);
    // appController->registerPanel(this);
    installEventFilter(this);

    connect(m_toolbarView, &ClipEditorToolBarView::editModeChanged,
            m_pianoRollEditorView->pianoRollView(), &PianoRollView::onEditModeChanged);

    connect(appModel, &AppModel::modelChanged, this, &ClipEditorView::onModelChanged);
    connect(appStatus, &AppStatus::activeClipIdChanged, this, &ClipEditorView::onActiveClipChanged);
}

void ClipEditorView::centerAt(const double tick, const double keyIndex) {
    m_pianoRollEditorView->pianoRollView()->graphicsView()->setViewportCenterAt(tick, keyIndex);
}

void ClipEditorView::centerAt(const double startTick, const double length, const double keyIndex) {
    const auto centerTick = startTick + length / 2;
    m_pianoRollEditorView->pianoRollView()->graphicsView()->setViewportCenterAt(centerTick,
                                                                                keyIndex);
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
        NoteView::setTrackColorIndex(trackRef->colorIndex());
        m_pianoRollEditorView->pianoRollView()->setTrackColorIndex(trackRef->colorIndex());
        m_trackColorConnection =
            connect(trackRef, &Track::propertyChanged, this, [this, trackRef] {
                NoteView::setTrackColorIndex(trackRef->colorIndex());
                m_pianoRollEditorView->pianoRollView()->setTrackColorIndex(trackRef->colorIndex());
                m_pianoRollEditorView->pianoRollView()->update();
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
        appController->setActivePanel(AppGlobal::ClipEditor);
    return QWidget::eventFilter(watched, event);
}

void ClipEditorView::moveToSingingClipState(SingingClip *clip) const {
    m_hasActiveClip = true;
    m_pianoRollEditorView->setVisible(true);
    m_pianoRollEditorView->setDataContext(clip);
    m_pianoRollEditorView->pianoRollView()->onEditModeChanged(m_toolbarView->editMode());
}

void ClipEditorView::moveToAudioClipState(const AudioClip *clip) const {
    Q_UNUSED(clip);
    m_hasActiveClip = true;
    m_pianoRollEditorView->setVisible(false);
    m_pianoRollEditorView->setDataContext(nullptr);
}

void ClipEditorView::moveToNullClipState() const {
    m_hasActiveClip = false;
    m_pianoRollEditorView->setVisible(false);
    m_pianoRollEditorView->setDataContext(nullptr);
}

void ClipEditorView::reset() {
    onActiveClipChanged(-1);
}