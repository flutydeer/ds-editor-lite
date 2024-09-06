//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorView.h"

#include "PhonemeView.h"
#include "Controller/AppController.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/TracksViewController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppStatus/AppStatus.h"
#include "ParamEditor/ParamEditorView.h"
#include "PianoRoll/PianoRollGraphicsView.h"
#include "PianoRoll/PianoRollView.h"
#include "ToolBar/ClipEditorToolBarView.h"

#include <QLabel>
#include <QMouseEvent>
#include <QSplitter>
#include <QVBoxLayout>

ClipEditorView::ClipEditorView(QWidget *parent) : PanelView(AppGlobal::ClipEditor, parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("ClipEditorView");

    m_toolbarView = new ClipEditorToolBarView;
    m_toolbarView->setVisible(false);

    m_pianoRollView = new PianoRollView;
    connect(m_toolbarView, &ClipEditorToolBarView::editModeChanged, m_pianoRollView,
            &PianoRollView::onEditModeChanged);

    m_paramEditorView = new ParamEditorView;

    connect(appModel, &AppModel::modelChanged, this, &ClipEditorView::onModelChanged);
    connect(appStatus, &AppStatus::activeClipIdChanged, this, &ClipEditorView::onActiveClipChanged);

    m_splitter = new QSplitter(Qt::Vertical);
    m_splitter->setObjectName("ClipEditorSplitter");
    m_splitter->setContentsMargins(0, 0, 0, 0);
    // m_splitter->setContentsMargins(6, 0, 6, 0);
    m_splitter->addWidget(m_pianoRollView);
    m_splitter->addWidget(m_paramEditorView);
    m_splitter->setCollapsible(0, false);
    // 让参数面板在剪辑编辑器调整高度时尽量保持高度不变，优先调整钢琴卷帘区域的高度
    m_splitter->setStretchFactor(0, 100);
    m_splitter->setStretchFactor(1, 1);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_toolbarView);
    mainLayout->addWidget(m_splitter);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({1, 1, 1, 1});
    setLayout(mainLayout);

    clipController->setView(this);
    appController->registerPanel(this);
    installEventFilter(this);
}

void ClipEditorView::centerAt(double tick, double keyIndex) {
    m_pianoRollView->graphicsView()->setViewportCenterAt(tick, keyIndex);
}

void ClipEditorView::centerAt(double startTick, double length, double keyIndex) {
    auto centerTick = startTick + length / 2;
    m_pianoRollView->graphicsView()->setViewportCenterAt(centerTick, keyIndex);
}

void ClipEditorView::onModelChanged() {
    m_toolbarView->setDataContext(nullptr);
    reset();
}

void ClipEditorView::onActiveClipChanged(int clipId) const {
    auto clip = appModel->findClipById(clipId);
    m_toolbarView->setDataContext(clip);
    clipController->setClip(clip);

    if (clip == nullptr)
        moveToNullClipState();
    else if (clip->clipType() == Clip::Singing)
        moveToSingingClipState(dynamic_cast<SingingClip *>(clip));
    else if (clip->clipType() == Clip::Audio)
        moveToAudioClipState(nullptr);
}

bool ClipEditorView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QMouseEvent::MouseButtonPress)
        appController->setActivePanel(AppGlobal::ClipEditor);
    return QWidget::eventFilter(watched, event);
}

void ClipEditorView::moveToSingingClipState(SingingClip *clip) const {
    m_toolbarView->setVisible(true);
    // m_pianoRollView->setVisible(true);
    m_splitter->setVisible(true);

    m_pianoRollView->setDataContext(clip);
    m_pianoRollView->onEditModeChanged(m_toolbarView->editMode());
}

void ClipEditorView::moveToAudioClipState(AudioClip *clip) const {
    Q_UNUSED(clip);
    m_toolbarView->setVisible(true);
    // m_pianoRollView->setVisible(false);
    m_splitter->setVisible(false);

    m_pianoRollView->setDataContext(nullptr);
}

void ClipEditorView::moveToNullClipState() const {
    m_toolbarView->setVisible(false);
    // m_pianoRollView->setVisible(false);
    m_splitter->setVisible(false);

    m_pianoRollView->setDataContext(nullptr);
}

void ClipEditorView::reset() {
    onActiveClipChanged(-1);
}

// void ClipEditorView::printParts() {
//     auto p = m_singingClip->parts();
//     if (p.count() > 0) {
//         int i = 0;
//         for (const auto &part : p) {
//             auto notes = part.info.selectedNotes;
//             if (notes.count() == 0)
//                 continue;
//             auto start = notes.first().start();
//             auto end = notes.last().start() + notes.last().length();
//             qDebug() << "Part" << i << ": [" << start << "," << end << "]" << notes.count();
//             i++;
//         }
//     }
// }