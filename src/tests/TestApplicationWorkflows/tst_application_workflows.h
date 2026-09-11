#pragma once

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppStatus/AppStatus.h"

#include <QObject>
#include <QTemporaryDir>

#include <memory>

class SingingClip;
class InferPiece;
class InferPipeline;
class Note;

class ApplicationWorkflowTests final : public QObject {
    Q_OBJECT

private slots:
    void audioExportRespectsRangeMixAndMute();
    void lossyAudioExportsProduceReadableFiles_data();
    void lossyAudioExportsProduceReadableFiles();
    void cancelingAudioExportPreservesExistingFilesAndMixer();
    void controlledPlaybackLoopsAndBuffers();
    void audioClipRangeChangesWaitForActiveReads_data();
    void audioClipRangeChangesWaitForActiveReads();
    void customExportPresetPersistsAndProducesIntegerWave();

    void initTestCase();

    void init();

    void changedTargetInputDropsResult_data();

    void speakerMixPresetPersistsThroughTheProductionStore();
    void publicSpeakerMixPresetsResolveAndPreserveAppliedVoices();

    void lyricRulesUseTheProductionRuntimeAndPersistence();

    void projectBatchImportUsesRealLoaders_data();

    void projectBatchImportUsesRealLoaders();
    void publicSingleProjectImportUsesThePreparedPlanAndKeepsTheDocument();

    void audioBatchFailurePolicy_data();

    void audioBatchFailurePolicy();

    void audioBatchCancellationReleasesRetry();
    void publicAudioPathUpdatesPrepareCommitAndUndo();

    void clipInferenceResultsRespectEditSession_data();

    void clipInferenceResultsRespectEditSession();
    void editingParametersRestartsOnlyDependentInference_data();
    void editingParametersRestartsOnlyDependentInference();
    void changingSpeakerMixRefreshesExistingInference();
    void playbackWindowPrioritizesAndSuspendsAcousticInference();
    void cancelingVoiceExportDuringPreparationAllowsAnotherExport();
    void movingInheritedVoiceReusesOrRebuildsInference_data();
    void movingInheritedVoiceReusesOrRebuildsInference();

    void failedInferenceInitializationReleasesPackageWaiters();

    void changedTargetInputDropsResult();

    void removedTargetDropsResult_data();

    void removedTargetDropsResult();

    void unchangedInputSurvivesRevisionDrift_data();

    void unchangedInputSurvivesRevisionDrift();

    void editSessionControlsResultDeferral_data();

    void editSessionControlsResultDeferral();

    void restartInferenceReleasesReplacedTask();
    void restartInferenceReleasesReplacedTask_data();

    void publicInferenceStartsBeforeQueuedDocumentChanges();
    void publicInferenceStatusAssociatesTasksWithTheirScope();

    void offlineExportRestoresMixerState_data();

    void offlineExportRestoresMixerState();

    void cleanup();

    void cleanupTestCase();

private:
    void prepareInferenceTarget(AppStatus::ModuleStatus &previousPackageStatus);
    void prepareVoicebankTarget();

    void verifyAcousticGate(InferPipeline &pipeline, bool immediateExpected, bool completeFirst);

    Automation::CoreRuntime &runtime();

    Automation::CommandContext commandContext();

    QTemporaryDir dataRoot;
    QByteArray previousDataRoot;
    bool dataRootInstalled = false;
    std::unique_ptr<AppContext> context;
    Automation::TrackId trackId;
    SingingClip *clip = nullptr;
    SingingClip *otherClip = nullptr;
    InferPiece *piece = nullptr;
    Note *note = nullptr;
};
