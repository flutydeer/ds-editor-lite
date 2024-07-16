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
#include "GraphicsItem/PitchEditorGraphicsItem.h"
#include "Model/AppModel.h"
#include "UI/Views/Common/TimelineView.h"

#include <QMouseEvent>
#include <QVBoxLayout>

ClipEditorView::ClipEditorView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("ClipEditorView");

    m_toolbarView = new ClipEditorToolBarView;
    connect(m_toolbarView, &ClipEditorToolBarView::editModeChanged, this,
            &ClipEditorView::onEditModeChanged);

    m_pianoRollScene = new PianoRollGraphicsScene;
    m_pianoRollView = new PianoRollGraphicsView(m_pianoRollScene);
    m_pianoRollView->setSceneVisibility(false);
    m_pianoRollView->setDragMode(QGraphicsView::RubberBandDrag);

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

    auto model = AppModel::instance();
    connect(model, &AppModel::modelChanged, this, &ClipEditorView::onModelChanged);
    connect(model, &AppModel::selectedClipChanged, this, &ClipEditorView::onSelectedClipChanged);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_toolbarView);
    mainLayout->addWidget(m_timelineView);
    mainLayout->addWidget(m_pianoRollView);
    mainLayout->addWidget(m_phonemeView);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({1, 1, 1, 1});
    setLayout(mainLayout);

    updateStyleSheet();
    ClipEditorViewController::instance()->setView(this);
    AppController::instance()->registerPanel(this);
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

    if (clip == nullptr) {
        qDebug() << "ClipEditorView::setIsSingingClip null";
        ClipEditorViewController::instance()->setCurrentSingingClip(nullptr);
        m_toolbarView->setClip(nullptr);
        m_toolbarView->setClipPropertyEditorEnabled(false);
        m_toolbarView->setPianoRollEditToolsEnabled(false);

        m_pianoRollView->setIsSingingClip(false);
        m_pianoRollView->setSceneVisibility(false);

        m_timelineView->setVisible(false);
        m_phonemeView->setVisible(false);
        disconnect(m_clip, &Clip::propertyChanged, this,
                       &ClipEditorView::onClipPropertyChanged);
        if (m_singingClip != nullptr) {
            disconnect(m_singingClip, &SingingClip::noteChanged, this,
                       &ClipEditorView::onNoteListChanged);
            disconnect(m_singingClip, &SingingClip::noteSelectionChanged, this,
                       &ClipEditorView::onNoteSelectionChanged);
        }
        m_clip = nullptr;
        m_singingClip = nullptr;
        m_pianoRollView->setEnabled(false);
        return;
    }

    m_pianoRollView->setEnabled(true);
    m_clip = clip;
    qDebug() << "connect track and ClipEditorView";
    connect(m_clip, &Clip::propertyChanged, this, &ClipEditorView::onClipPropertyChanged);
    connect(m_pianoRollView, &TimeGraphicsView::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    m_toolbarView->setClip(clip);
    m_toolbarView->setClipPropertyEditorEnabled(true);

    if (clip->type() != Clip::Singing)
        return;
    m_toolbarView->setPianoRollEditToolsEnabled(true);
    m_pianoRollView->setIsSingingClip(true);
    m_pianoRollView->setSceneVisibility(true);
    m_timelineView->setVisible(true);
    m_phonemeView->setVisible(true);
    m_singingClip = dynamic_cast<SingingClip *>(m_clip);
    m_phonemeView->setSingingClip(m_singingClip);
    if (m_singingClip->notes().count() > 0) {
        for (const auto note : m_singingClip->notes()) {
            m_pianoRollView->insertNote(note);
            m_phonemeView->insertNote(note);
            m_notes.append(note);
        }
        auto firstNote = m_singingClip->notes().at(0);
        qDebug() << "first note start" << firstNote->start();
        m_pianoRollView->setViewportCenterAt(firstNote->start(), firstNote->keyIndex());
    } else
        m_pianoRollView->setViewportCenterAtKeyIndex(60);
    connect(m_singingClip, &SingingClip::noteChanged, this, &ClipEditorView::onNoteListChanged);
    connect(m_singingClip, &SingingClip::noteSelectionChanged, this,
            &ClipEditorView::onNoteSelectionChanged);
    connect(m_singingClip, &SingingClip::paramChanged, this, &ClipEditorView::onParamChanged);
    ClipEditorViewController::instance()->setCurrentSingingClip(m_singingClip);
}
void ClipEditorView::onEditModeChanged(PianoRollEditMode mode) {
    m_mode = mode;
    m_pianoRollView->setEditMode(m_mode);
}
void ClipEditorView::onParamChanged(ParamBundle::ParamName paramName, Param::ParamType paramType) {
    if (paramName == ParamBundle::Pitch) {
        auto pitchParam = m_singingClip->params.getParamByName(paramName);
        m_pianoRollView->updatePitch(paramType, *pitchParam);
    }
}
bool ClipEditorView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QMouseEvent::MouseButtonPress) {
        // qDebug() << "ClipEditorView MouseButtonPress";
        AppController::instance()->onPanelClicked(AppGlobal::ClipEditor);
    }

    return QWidget::eventFilter(watched, event);
}
void ClipEditorView::reset() {
    m_pianoRollView->reset();
    m_phonemeView->reset();
}
void ClipEditorView::onClipPropertyChanged() {
    qDebug() << "ClipEditorView::handleClipPropertyChange" << m_clip->id() << m_clip->start();
    auto singingClip = dynamic_cast<SingingClip *>(m_clip);
    if (!singingClip)
        return;
    if (singingClip->notes().count() <= 0)
        return;
    for (const auto note : singingClip->notes()) {
        m_pianoRollView->updateNoteTimeAndKey(note);
        m_phonemeView->updateNoteTime(note);
    }
}
void ClipEditorView::onNoteListChanged(SingingClip::NoteChangeType type, int id, Note *note) {
    switch (type) {
        case SingingClip::Inserted:
            m_pianoRollView->insertNote(note);
            m_phonemeView->insertNote(note);
            connect(note, &Note::propertyChanged, this,
                    [=](Note::NotePropertyType type) { onNotePropertyChanged(type, note); });
            break;
        case SingingClip::Removed:
            m_pianoRollView->removeNote(id);
            m_phonemeView->removeNote(id);
            break;
    }
    m_pianoRollView->updateOverlappedState(m_singingClip);
    // printParts();
}
void ClipEditorView::onNotePropertyChanged(Note::NotePropertyType type, Note *note) {
    switch (type) {
        case Note::TimeAndKey:
            m_pianoRollView->updateNoteTimeAndKey(note);
            m_pianoRollView->updateOverlappedState(m_singingClip);
            m_phonemeView->updateNoteTime(note);
            // printParts();
            break;
        case Note::Word:
            m_pianoRollView->updateNoteWord(note);
            m_phonemeView->updateNotePhonemes(note);
            break;
        case Note::None:
            break;
    }
}
void ClipEditorView::onNoteSelectionChanged() {
    auto selectedNotes = m_singingClip->selectedNotes();
    m_pianoRollView->updateNoteSelection(selectedNotes);
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
void ClipEditorView::afterSetActivated() {
    updateStyleSheet();
}
void ClipEditorView::updateStyleSheet() {
    auto borderStyle = panelActivated() ? "border: 1px solid rgb(126, 149, 199);"
                                        : "border: 1px solid rgb(20, 20, 20);";
    setStyleSheet(QString("QWidget#ClipEditorView {background: #2A2B2C; border-radius: 6px; ") +
                  borderStyle + "}");
}