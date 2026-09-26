#ifndef NATIVEDESKTOPTESTS_H
#define NATIVEDESKTOPTESTS_H

#include "../TestSupport/ProjectSnapshot.h"

#include <QObject>
#include <QStringList>
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
    void customWindowButtonsKeepTheDetachedPanelAndDocument();
    void mainWindowSplitterDragRestoresPanelSizes();
    void closingMainWindowWaitsForBackgroundTasks_data();
    void closingMainWindowWaitsForBackgroundTasks();
    void dragGrip_data();
    void dragGrip();
    void effectiveDurationPolicy();
    void dialogTitleBarRuntimeUpdate();
    void progressAndTapTempoLevels();
    void toolTipImmediateCompletion();
    void toolTipAnchorScreenClamping();
    void rhiThemeSwitchPreservesBothEditorsAndTheirDocument();
    void rhiNoteDrawingCommitsAndUndoUpdatesInteraction_data();
    void rhiNoteDrawingCommitsAndUndoUpdatesInteraction();
    void rhiNoteMoveCanBeCanceledAndThenCommitted();
    void rhiNoteDragKeepsScrollingUntilTheGestureEnds();
    void rhiMultiNoteSelectionAndMoveCommitAtomically();
    void rhiNoteResizeUndoRestoresTheHitRegion_data();
    void rhiNoteResizeUndoRestoresTheHitRegion();
    void rhiNoteEraseStrokeCancelsAndCommitsAtomically();
    void rhiInlineTextEditingNavigatesCancelsAndUndoes();
    void rhiPitchStrokePreviewsCancelAndCommit_data();
    void rhiPitchStrokePreviewsCancelAndCommit();
    void rhiNoteSplittingSnapsAndUndoRestoresThePhrase();
    void rhiContextMenuTargetsRespectPronunciationAndSelection();
    void rhiPianoMenuPasteAndVisibilityUseTheFullEditor();
    void rhiPianoWheelInputsReachTheActiveViewport();
    void rhiPitchModulationUsesTheInferredBaseline();
    void rhiPitchAnchorInsertionAndCanceledDragUseTheRealEditor_data();
    void rhiPitchAnchorInsertionAndCanceledDragUseTheRealEditor();
    void rhiClipDragCommitsAcrossTracksAndUndoRestoresView();
    void rhiClipDragScrollsAtTheEdgeAndStopsOnCancel();
    void rhiClipResizeCommitsOrCancels_data();
    void rhiClipResizeCommitsOrCancels();
    void rhiAudioClipTrimAndMovePreserveTimeAnchors();
    void rhiTrackMenuPasteAndSelectionUseTheFullEditor_data();
    void rhiTrackMenuPasteAndSelectionUseTheFullEditor();
    void rhiTrackFileDropImportsAtTheChosenSlot_data();
    void rhiTrackFileDropImportsAtTheChosenSlot();
    void rhiFileDropScrollsUntilTheDragLeaves();
    void availableAudioDeviceRunsPublicPlayback();
    void audioSettingsRollbackWithoutAnInitializedBackend();
    void failedAudioDriverSelectionClearsTheReleasedDevice();
    void audioDriverStartupCanBeCanceled_data();
    void audioDriverStartupCanBeCanceled();
    void configuredMidiLoopbackFeedsLiveSynthesizer();

private:
    void runIsolatedDesktopCase();
    bool eventFilter(QObject *object, QEvent *event) override;
    std::unique_ptr<GuiAppFixture> application;
    QStringList recentInput;
};

#endif
