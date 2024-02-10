//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorView.h"

#include <QVBoxLayout>
ClipEditorView::ClipEditorView(QWidget *parent) : QWidget(parent) {
    m_toolbarView = new ClipEditorToolBarView;
    m_pianoRollView = new PianoRollGraphicsView;

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_toolbarView);
    mainLayout->addWidget(m_pianoRollView);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({});
    setLayout(mainLayout);
}
void ClipEditorView::onModelChanged() {
    m_pianoRollView->setClip(nullptr, nullptr);
}
void ClipEditorView::onSelectedClipChanged(DsTrack *track, DsClip *clip) {
    m_track = track;
    m_clip = clip;
    m_pianoRollView->setClip(track, clip);
}