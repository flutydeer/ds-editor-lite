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
    void controlledPlaybackLoopsAndBuffers();

    void initTestCase();

    void init();

    void changedTargetInputDropsResult_data();

    void speakerMixPresetPersistsThroughTheProductionStore();

    void lyricRulesUseTheProductionRuntimeAndPersistence();

    void projectBatchImportUsesRealLoaders_data();

    void projectBatchImportUsesRealLoaders();

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

    void offlineExportRestoresMixerState_data();

    void offlineExportRestoresMixerState();

    void cleanup();

    void cleanupTestCase();

private:
    void prepareInferenceTarget(AppStatus::ModuleStatus &previousPackageStatus);

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
