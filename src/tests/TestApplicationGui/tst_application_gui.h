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
    void draggingExistingNoteCommitsOrCancels_data();
    void draggingExistingNoteCommitsOrCancels();
    void trackClipDragCommitsOrCancels_data();
    void trackClipDragCommitsOrCancels();
    void parameterStrokeCommitsOnceAndUndoRestoresView();
    void escapeCancelsParameterStrokeWithoutChangingDocument();
    void parameterTransformGesturesCommitAndCancel_data();
    void parameterTransformGesturesCommitAndCancel();
    void inlineLyricsCommitNavigateAndCancel();
    void inlinePronunciationCommitsAndCancels();
    void phonemeBoundaryDragCommitsAndUndoRestoresOffsets();
    void phonemeWaveformsLoadAndDiscardResultsAfterChangingClips();
    void exportFormatUpdatesFileNamePreview();
    void exportSourcesAndMixingUpdateFilePlan();
    void canceledExportConfigurationDoesNotPersist();
    void audioExportProgressCompletesAndCloses();
    void appearanceInputsPersistAcrossReopening();
    void automationAccessInputsPersistAndRejectMissingFolders();
    void inferenceInputsPersistAcrossReopening();
    void cacheCleanupRequiresConfirmationAndRefreshesThePage();
    void interactiveProjectImportRespectsSelectionAndCancellation_data();
    void interactiveProjectImportRespectsSelectionAndCancellation();
    void droppingAudioFilesCommitsOneBatchToTheSelectedTracks();
    void fillLyricPreviewCommitsOrCancels_data();
    void fillLyricPreviewCommitsOrCancels();
    void lyricRuleEditingChangesThePreviewAndPersists();
    void phonemeDialogValidatesCommitsAndResetsThroughTheNoteMenu();
    void lyricSearchNavigatesTheActualEditorAndHandlesNoMatches();
    void dynamicSpeakerMixGesturesPreserveIdentityAndUndo();
    void timelineGesturesSeekAndCommitLoopEdits();
    void resizingANotePreviewsAndCommitsItsBoundary();
    void speakerMixSelectionAndDrag_data();
    void speakerMixSelectionAndDrag();
    void speakerMixPresetsFollowSaveSelectAndDeleteInputs();
    void packageSearchShowsTheSelectedPackageDetails();
    void missingAudioResourceRelinkCanBeCanceledAndCommitted();
    void panelButtonsAndClipDoubleClickRestoreTheEditorView();
    void detachedBottomPanelReattachesWithItsEditingContext();
    void embeddedSettingsSuspendAndRestoreBackgroundInteraction();
    void logWindowFiltersLiveMessagesAndCopiesDisplayedOrder();
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
