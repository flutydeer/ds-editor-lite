#pragma once

#include <QObject>

class ProjectEditingTests final : public QObject {
    Q_OBJECT

private slots:
    void batchAnchorsCommitAndUndoTogether();
    void adjacentAnchorCurvesMergeWithoutLosingNodes();
    void dynamicSpeakerKeyframesEditAndUndo();
    void batchTrackOrderAndClipTrimming();
    void noteSearch_data();
    void noteSearch();
    void splitAtPreservesPhraseAndUndo();
    void trackEditing();
    void singingClipEditing();
    void legacyAudioClipEditing();
    void trackRemovalRestoresChildren();
    void listNotes();
    void insertNotes();
    void moveNotes();
    void resizeNotesLeft();
    void resizeNotesRight();
    void splitNote();
    void setPhonemeOffsets();
    void resetPhonemeOffsetsCascades();
    void setWordProperties();
    void removeNotes();
    void curveTransforms_data();
    void curveTransforms();
    void parameterEditing();
    void speakerMixEditing();
    void timelineAndHistoryDomain();
    void quantizeInChild();
    void wholeClipParameterRoundTrip_data();
    void wholeClipParameterRoundTrip();
    void wholeClipsPasteAcrossTracksAsOneEdit();
    void duplicateWithParameters();
    void oversizedTargetTailIsRejected();
    void oversizedSourceCurveIsRejected();
    void insertReturnsCommittedNote();
    void rejectedInsertPreservesDocument();
    void splitReturnsCommittedChild();
    void rejectedSplitPreservesDocument();
};
