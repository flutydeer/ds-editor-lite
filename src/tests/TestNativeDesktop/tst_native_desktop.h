#ifndef NATIVEDESKTOPTESTS_H
#define NATIVEDESKTOPTESTS_H

#include <QObject>

class NativeDesktopTests final : public QObject {
    Q_OBJECT

private slots:
    void visibilityAndCollapsedPane();
    void reparentAndDestructionKeepGripOwnership();
    void dragGrip_data();
    void dragGrip();
    void effectiveDurationPolicy();
    void dialogTitleBarRuntimeUpdate();
    void progressAndTapTempoLevels();
    void toolTipImmediateCompletion();
    void toolTipAnchorScreenClamping();
    void rhiNoteDrawingCommitsAndUndoUpdatesInteraction();
    void rhiNoteMoveCanBeCanceledAndThenCommitted();
    void rhiNoteResizeUndoRestoresTheHitRegion();
    void rhiNoteEraseStrokeCancelsAndCommitsAtomically();
    void rhiInlineTextEditingNavigatesCancelsAndUndoes();
    void rhiPitchAnchorInsertionAndCanceledDragUseTheRealEditor();
    void rhiClipDragCommitsAcrossTracksAndUndoRestoresView();
    void rhiClipResizeCommitsOrCancels_data();
    void rhiClipResizeCommitsOrCancels();
    void availableAudioDeviceRunsPublicPlayback();
    void audioDriverStartupCanBeCanceled_data();
    void audioDriverStartupCanBeCanceled();
    void configuredMidiLoopbackFeedsLiveSynthesizer();
};

#endif
