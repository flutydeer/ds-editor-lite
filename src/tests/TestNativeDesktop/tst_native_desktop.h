#ifndef NATIVEDESKTOPTESTS_H
#define NATIVEDESKTOPTESTS_H

#include <QObject>
#include <memory>

class GuiAppFixture;

class NativeDesktopTests final : public QObject {
    Q_OBJECT

public:
    NativeDesktopTests();
    ~NativeDesktopTests() override;

private slots:
    void initTestCase();
    void cleanupTestCase();
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
    void rhiMultiNoteSelectionAndMoveCommitAtomically();
    void rhiNoteResizeUndoRestoresTheHitRegion();
    void rhiNoteEraseStrokeCancelsAndCommitsAtomically();
    void rhiInlineTextEditingNavigatesCancelsAndUndoes();
    void rhiPitchStrokePreviewsCancelAndCommit_data();
    void rhiPitchStrokePreviewsCancelAndCommit();
    void rhiNoteSplittingSnapsAndUndoRestoresThePhrase();
    void rhiContextMenuTargetsRespectPronunciationAndSelection();
    void rhiPianoMenuPasteAndVisibilityUseTheFullEditor();
    void rhiPitchModulationUsesTheInferredBaseline();
    void rhiPitchAnchorInsertionAndCanceledDragUseTheRealEditor();
    void rhiClipDragCommitsAcrossTracksAndUndoRestoresView();
    void rhiClipResizeCommitsOrCancels_data();
    void rhiClipResizeCommitsOrCancels();
    void rhiAudioClipTrimAndMovePreserveTimeAnchors();
    void rhiTrackMenuPasteAndSelectionUseTheFullEditor();
    void rhiTrackFileDropImportsAtTheChosenSlot_data();
    void rhiTrackFileDropImportsAtTheChosenSlot();
    void availableAudioDeviceRunsPublicPlayback();
    void audioSettingsRollbackWithoutAnInitializedBackend();
    void audioDriverStartupCanBeCanceled_data();
    void audioDriverStartupCanBeCanceled();
    void configuredMidiLoopbackFeedsLiveSynthesizer();

private:
    std::unique_ptr<GuiAppFixture> application;
};

#endif
