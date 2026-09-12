#ifndef APPLICATIONGUITESTS_H
#define APPLICATIONGUITESTS_H

#include "Automation/AutomationTypes.h"

#include <QObject>
#include <QPoint>
#include <QTemporaryDir>

#include <memory>

class AppContext;
class QMimeData;
class PianoRollGraphicsScene;
class PianoRollGraphicsView;
class SingingClip;
class NoteView;

class ApplicationGuiTests final : public QObject {
    Q_OBJECT

public:
    ApplicationGuiTests();
    ~ApplicationGuiTests() override;

private slots:
    void initTestCase();
    void init();
    void copyPasteUsesTheActiveClipAndPlaybackPosition();
    void cutCopiesThenRemovesSelectionAsOneUndoStep();
    void wholeClipClipboardUsesSelectedTrackAndPreservesCurves();
    void publicPlaybackDeviceFailureDoesNotOpenAModalDialog();
    void invalidClipboardDoesNotEdit_data();
    void invalidClipboardDoesNotEdit();
    void drawingCommitsOnceAndUndoRedoUpdatesTheScene();
    void intervalSelectionUsesTheToolbarAndDeletesOnlyTheChosenTimeRange_data();
    void intervalSelectionUsesTheToolbarAndDeletesOnlyTheChosenTimeRange();
    void pianoErasingRestoresSceneItemsOnCancelAndUndo();
    void pianoSplitIndicatorFollowsTheMouseAndMatchesTheEdit();
    void draggingExistingNoteCommitsOrCancels_data();
    void draggingExistingNoteCommitsOrCancels();
    void trackClipDragCommitsOrCancels_data();
    void trackClipDragCommitsOrCancels();
    void parameterStrokeCommitsOnceAndUndoRestoresView();
    void parameterAnchorEditingPreviewsAndUsesTheContextMenu();
    void parameterToolbarSwapsTheVisiblePairWithoutEditingTheDocument();
    void escapeCancelsParameterStrokeWithoutChangingDocument();
    void pitchAnchorCreationPreviewsBeforeCommitting();
    void pitchAnchorRangeEditsUseTheViewAndMenu();
    void pitchAnchorMergePreviewCommitsAndUndoes();
    void parameterTransformGesturesCommitAndCancel_data();
    void parameterTransformGesturesCommitAndCancel();
    void parameterTransformHandlesControlTheTransitionRange();
    void inlineLyricsCommitNavigateAndCancel();
    void inlinePronunciationCommitsAndCancels();
    void pronunciationMenuChangesOnlyTheClickedNote();
    void phonemeBoundaryDragCommitsAndUndoRestoresOffsets();
    void phonemeWaveformsLoadAndDiscardResultsAfterChangingClips();
    void exportFormatUpdatesFileNamePreview();
    void exportSourcesAndMixingUpdateFilePlan();
    void canceledExportConfigurationDoesNotPersist();
    void exportPresetDialogsSaveOverwriteAndDeleteTheSelectedPreset();
    void audioExportProgressCompletesAndCloses();
    void appearanceInputsPersistAcrossReopening();
    void generalSettingsKeepSeparateDefaultLyricsForEachLanguage();
    void switchingUiLanguagePreservesSettingsAndTheOpenDocument();
    void experimentalRendererSettingPersistsWhenRestartIsDeferred();
    void automationAccessInputsPersistAndRejectMissingFolders();
    void automationCustomToolsetInputsPersistAndExportPermissions();
    void automationServerReconfigurationUpdatesAccessAndConnectionDetails();
    void inferenceInputsPersistAcrossReopening();
    void cacheCleanupRequiresConfirmationAndRefreshesThePage();
    void interactiveProjectImportRespectsSelectionAndCancellation_data();
    void interactiveProjectImportRespectsSelectionAndCancellation();
    void midiChannelSelectionRebuildsTracksBeforeImport_data();
    void midiChannelSelectionRebuildsTracksBeforeImport();
    void droppingAudioFilesCommitsOneBatchToTheSelectedTracks();
    void droppingMidiAndAudioFilesUsesOneBatchDecision_data();
    void droppingMidiAndAudioFilesUsesOneBatchDecision();
    void fillLyricPreviewCommitsOrCancels_data();
    void fillLyricPreviewCommitsOrCancels();
    void lyricRuleEditingChangesThePreviewAndPersists();
    void phonemeDialogValidatesCommitsAndResetsThroughTheNoteMenu();
    void phonemeDurationResetConfirmsAdjacentChanges_data();
    void phonemeDurationResetConfirmsAdjacentChanges();
    void noteLanguageMenuChangesOnlyTheSelectedWords_data();
    void noteLanguageMenuChangesOnlyTheSelectedWords();
    void lyricSearchNavigatesTheActualEditorAndHandlesNoMatches();
    void dynamicSpeakerMixGesturesPreserveIdentityAndUndo();
    void dynamicSpeakerMixRangeDeletionAndContextMenu();
    void dynamicSpeakerMixNavigationUsesProjectTime_data();
    void dynamicSpeakerMixNavigationUsesProjectTime();
    void dynamicSpeakerMixBypassAndStopFollowToolbarInputs();
    void timelineGesturesSeekAndCommitLoopEdits();
    void resizingANotePreviewsAndCommitsItsBoundary_data();
    void resizingANotePreviewsAndCommitsItsBoundary();
    void speakerMixSelectionAndDrag_data();
    void speakerMixSelectionAndDrag();
    void speakerMixModifierDragPreservesGroupRatios();
    void speakerMixPresetsFollowSaveSelectAndDeleteInputs();
    void voiceMenusApplyPresetsToTheChosenTarget_data();
    void voiceMenusApplyPresetsToTheChosenTarget();
    void clipToolbarNameEditingKeepsTheOriginalTarget();
    void packageSearchShowsTheSelectedPackageDetails();
    void missingAudioResourceRelinkCanBeCanceledAndCommitted();
    void audioResourceConfirmationKeepsTheDecodedSource();
    void panelButtonsAndClipDoubleClickRestoreTheEditorView();
    void closingTheMainWindowReleasesTheDefaultDialogParent();
    void mainMenuQuantizationUsesTheChosenScopeAndOptions_data();
    void mainMenuQuantizationUsesTheChosenScopeAndOptions();
    void mainMenuOctaveEditsFollowThePianoSelection();
    void editorAutomationConfiguresTheVisibleWorkspaceWithoutEditingTheDocument();
    void undoShortcutRevealsTheTrackEditBeforeChangingIt_data();
    void undoShortcutRevealsTheTrackEditBeforeChangingIt();
    void undoShortcutRevealsThePianoEditBeforeChangingIt();
    void fileMenuOpensAndSavesThroughTheActualPicker();
    void recentProjectsMenuRemovesMissingFilesAndClearsTheList();
    void titleFilePopupOpensProjectsAndRemovesOnlyRecentEntries();
    void failedProjectOpenPreservesTheDocumentAndRecovers();
    void projectDropCanCancelThenOpenTheDocument();
    void mixedFileDropRejectsAtomicallyAndAllowsTheNextImport();
    void detachedBottomPanelReattachesWithItsEditingContext();
    void embeddedSettingsSuspendAndRestoreBackgroundInteraction();
    void logWindowFiltersLiveMessagesAndCopiesDisplayedOrder();
    void newDocumentHonorsTheSaveDecision_data();
    void newDocumentHonorsTheSaveDecision();
    void rejectedProjectInputAllowsTheNextRequest_data();
    void rejectedProjectInputAllowsTheNextRequest();
    void pendingProjectLoadCanCancelOrRequestExit_data();
    void pendingProjectLoadCanCancelOrRequestExit();
    void taggerRuleInputsApplyPersistAndReopen();
    void invalidTaggerRegexPreservesAppliedRules();
    void movingLyricsBackwardUsesTheSelectedWordRange_data();
    void movingLyricsBackwardUsesTheSelectedWordRange();
    void pianoKeyboardGlissandoAndHideReleasePressedNotes();
    void pianoKeyboardRangeAndScrollingFollowTheEditor();
    void trackContextMenuPastePreviewCancelsAndMatchesCommittedClip();
    void settingsSynthPreviewKeepsEnvelopeDurationsAcrossSampleRates();
    void audioPageInputsPersistWithoutPlayback();
    void audioSettingsSaveFailureRestoresRuntimeAndAllowsRetry();
    void midiPageSynthInputsPersistWithoutPlayback();
    void trackHeaderInputsCommitAndUndo();
    void playbackPopupsEditTheMarkerChosenWhenTheyOpen();
    void playbackTextInputsValidateCommitAndCancel();
    void tapTempoMeasuresASequenceAndResetsAfterInactivity();
    void mixerChannelInputsAndLevelsStayScoped_data();
    void mixerChannelInputsAndLevelsStayScoped();
    void trackColorMenuPreviewsAndCommits_data();
    void trackColorMenuPreviewsAndCommits();
    void pitchModulationUsesTheInferredNoteBaselineAndCanBeUndone();
    void pianoContextMenuPastePreservesRelativeNotesAndManualWords();
    void pianoNoteDragContinuesDuringEdgeScrollingAndStopsOnFinish();
    void lyricGridSelectionDeletesOnlyChosenWords_data();
    void lyricGridSelectionDeletesOnlyChosenWords();
    void lyricGridMovesSelectedLinesTogether_data();
    void lyricGridMovesSelectedLinesTogether();
    void lyricGridSplitKeepsTheNewLineEditable();
    void lyricGridMenusInsertAndClearWords();
    void trackMenusCreateCutAndDeleteWithUndo();
    void trackAudioMenuPreparesClipOrCancels_data();
    void trackAudioMenuPreparesClipOrCancels();
    void cleanup();
    void cleanupTestCase();

private:
    void createPianoRoll();
    void createExportTracks();
    QString createWaveFixture(const QString &path) const;
    void createLyricSelection();
    int insertSelectedNote();
    Automation::CommandContext commandContext() const;
    QPoint pointFor(int tick, int key) const;
    int sceneNoteCount(int id) const;
    const NoteView *sceneNote(int id) const;

    QTemporaryDir dataRoot;
    QByteArray previousDataRoot;
    bool dataRootInstalled = false;
    std::unique_ptr<AppContext> context;
    std::unique_ptr<PianoRollGraphicsScene> scene;
    std::unique_ptr<PianoRollGraphicsView> view;
    SingingClip *singingClip = nullptr;
    Automation::TrackId trackId;
    std::unique_ptr<QMimeData> savedClipboard;
};

#endif
