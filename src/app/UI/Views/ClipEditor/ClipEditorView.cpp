//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorView.h"

#include "ClipEditorToolBarView.h"
#include "PhonemeView.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"
#include "Controller/AppController.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/TracksViewController.h"
#include "Model/AppModel.h"
#include "UI/Views/Common/TimelineView.h"

#include <QMouseEvent>
#include <QVBoxLayout>
#include <QStyle>

ClipEditorView::ClipEditorView(QWidget *parent) : PanelView(AppGlobal::ClipEditor, parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("ClipEditorView");

    m_toolbarView = new ClipEditorToolBarView;
    m_toolbarView->setVisible(false);

    m_pianoRollScene = new PianoRollGraphicsScene;
    m_pianoRollView = new PianoRollGraphicsView(m_pianoRollScene);
    m_pianoRollView->setSceneVisibility(false);
    m_pianoRollView->setDragMode(QGraphicsView::RubberBandDrag);
    connect(m_toolbarView, &ClipEditorToolBarView::editModeChanged, m_pianoRollView,
            &PianoRollGraphicsView::onEditModeChanged);

    m_timelineView = new TimelineView;
    m_timelineView->setTimeRange(m_pianoRollView->startTick(), m_pianoRollView->endTick());
    m_timelineView->setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    m_timelineView->setFixedHeight(timelineViewHeight);
    m_timelineView->setVisible(false);
    connect(m_timelineView, &TimelineView::wheelHorScale, m_pianoRollView,
            &CommonGraphicsView::onWheelHorScale);
    connect(m_pianoRollView, &TimeGraphicsView::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);

    m_phonemeView = new PhonemeView;
    m_phonemeView->setTimeRange(m_pianoRollView->startTick(), m_pianoRollView->endTick());
    m_phonemeView->setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    m_phonemeView->setFixedHeight(40);
    m_phonemeView->setVisible(false);
    connect(m_pianoRollView, &TimeGraphicsView::timeRangeChanged, m_phonemeView,
            &PhonemeView::setTimeRange);

    connect(appModel, &AppModel::modelChanged, this, &ClipEditorView::onModelChanged);
    connect(appModel, &AppModel::selectedClipChanged, this, &ClipEditorView::onSelectedClipChanged);

    // auto pianoKeyboardView = new PianoKeyboardView;
    // pianoKeyboardView->setKeyIndexRange(m_pianoRollView->topKeyIndex(),
    //                                     m_pianoRollView->bottomKeyIndex());
    // connect(m_pianoRollView, &PianoRollGraphicsView::keyIndexRangeChanged, pianoKeyboardView,
    //         &PianoKeyboardView::setKeyIndexRange);

    // auto pianoViewLayout = new QHBoxLayout;
    // pianoViewLayout->setSpacing(0);
    // pianoViewLayout->setContentsMargins(0, 0, 0, 0);
    // pianoViewLayout->addWidget(pianoKeyboardView);
    // pianoViewLayout->addWidget(m_pianoRollView);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_toolbarView);
    mainLayout->addWidget(m_timelineView);
    mainLayout->addWidget(m_pianoRollView);
    // mainLayout->addLayout(pianoViewLayout);
    mainLayout->addWidget(m_phonemeView);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({1, 1, 1, 1});
    setLayout(mainLayout);

    clipController->setView(this);
    appController->registerPanel(this);
    installEventFilter(this);
}
void ClipEditorView::centerAt(double tick, double keyIndex) {
    m_pianoRollView->setViewportCenterAt(tick, keyIndex);
}
void ClipEditorView::centerAt(double startTick, double length, double keyIndex) {
    auto centerTick = startTick + length / 2;
    m_pianoRollView->setViewportCenterAt(centerTick, keyIndex);
}
void ClipEditorView::onModelChanged() {
    reset();
}
void ClipEditorView::onSelectedClipChanged(Clip *clip) {
    reset();
    m_toolbarView->setDataContext(clip);
    clipController->setClip(clip);

    // no clip selected
    if (clip == nullptr) {
        m_toolbarView->setVisible(false);
        m_pianoRollView->setDataContext(nullptr);
        m_timelineView->setVisible(false);
        m_phonemeView->setVisible(false);
        return;
    }

    // one clip selected
    m_toolbarView->setVisible(true);
    if (clip->type() == Clip::Singing) {
        m_timelineView->setVisible(true);
        m_phonemeView->setVisible(true);
        auto singingClip = dynamic_cast<SingingClip *>(clip);
        m_pianoRollView->setDataContext(singingClip);
        m_phonemeView->setSingingClip(singingClip);
    } else if (clip->type() == Clip::Audio) {
        m_pianoRollView->setDataContext(nullptr);
        m_timelineView->setVisible(false);
        m_phonemeView->setVisible(false);
    }
}
bool ClipEditorView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QMouseEvent::MouseButtonPress)
        appController->onPanelClicked(AppGlobal::ClipEditor);
    return QWidget::eventFilter(watched, event);
}
void ClipEditorView::reset() {
    m_pianoRollView->reset();
    m_phonemeView->reset();
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