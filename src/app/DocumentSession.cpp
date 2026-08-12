#include "DocumentSession.h"

#include "AppContext.h"
#include "Controller/AppController.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/ClipboardController.h"
#include "Controller/ClipController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/EditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Controller/ProjectPackageResolver.h"
#include "Controller/ProjectStatusController.h"
#include "Controller/TrackController.h"
#include "Controller/UndoRedoController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/Utils/ParamUtils.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Extractors/MidiExtractController.h"
#include "Modules/Extractors/PitchExtractController.h"
#include "Modules/Import/DocumentImportController.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/InferController.h"
#include "Modules/ProjectFormats/ProjectFormatRegistry.h"
#include "UI/Controls/LevelMeterManager.h"

#include <lite/Core/SingletonRegistry.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/Tasking/TaskManager.h>

DocumentSession::DocumentSession(QObject *parent) : QObject(parent) {
    AppContext::activateSession(this);

    m_appStatus = SingletonRegistry::create<AppStatus>();
    m_appStatus->setParent(this);
    m_appModel = SingletonRegistry::create<AppModel>();
    m_appModel->setParent(this);
    m_paramUtils = SingletonRegistry::create<ParamUtils>();
    m_paramUtils->setParent(this);
    m_historyManager = SingletonRegistry::create<HistoryManager>();
    m_historyManager->setParent(this);
    m_levelMeterManager = SingletonRegistry::create<LevelMeterManager>(m_appModel);
    m_levelMeterManager->setParent(this);

    m_audioDecodingController = SingletonRegistry::create<AudioDecodingController>();
    m_audioDecodingController->setParent(this);
    m_clipboardController = SingletonRegistry::create<ClipboardController>();
    m_clipboardController->setParent(this);
    m_trackController = SingletonRegistry::create<TrackController>();
    m_trackController->setParent(this);
    m_clipController = SingletonRegistry::create<ClipController>();
    m_clipController->setParent(this);
    m_editorViewController = SingletonRegistry::create<EditorViewController>();
    m_editorViewController->setParent(this);
    m_undoRedoController = SingletonRegistry::create<UndoRedoController>();
    m_undoRedoController->setParent(this);
    m_pitchExtractController = SingletonRegistry::create<PitchExtractController>();
    m_pitchExtractController->setParent(this);
    m_midiExtractController = SingletonRegistry::create<MidiExtractController>();
    m_midiExtractController->setParent(this);
    m_editSessionManager = SingletonRegistry::create<EditSessionManager>();
    m_editSessionManager->setParent(this);
    m_playbackController = SingletonRegistry::create<PlaybackController>();
    m_playbackController->setParent(this);
    m_projectStatusController = SingletonRegistry::create<ProjectStatusController>();
    m_projectStatusController->setParent(this);
    m_projectPackageResolver = SingletonRegistry::create<ProjectPackageResolver>();
    m_projectPackageResolver->setParent(this);
    m_inferController = SingletonRegistry::create<InferController>();
    m_inferController->setParent(this);
    m_projectFormatRegistry = ProjectFormatRegistry::instance();
    m_documentImportController = SingletonRegistry::create<DocumentImportController>();
    m_documentImportController->setParent(this);
    m_audioContext = std::make_unique<AudioContext>();
    SingletonRegistry::add(m_audioContext.get());
    m_appController = SingletonRegistry::create<AppController>();
    m_appController->setParent(this);
    m_documentWorkflowController = SingletonRegistry::create<DocumentWorkflowController>();
    m_documentWorkflowController->setParent(this);
}

DocumentSession::~DocumentSession() {
    activate();
    SingletonRegistry::destroy(m_documentWorkflowController);
    SingletonRegistry::destroy(m_appController);

    auto *tasks = TaskManager::instance();
    tasks->terminateTasks(m_id);
    tasks->waitForDocument(m_id);

    SingletonRegistry::remove<AudioContext>();
    m_audioContext.reset();
    SingletonRegistry::destroy(m_documentImportController);
    SingletonRegistry::destroy(m_inferController);
    SingletonRegistry::destroy(m_projectPackageResolver);
    SingletonRegistry::destroy(m_projectStatusController);
    SingletonRegistry::destroy(m_playbackController);
    SingletonRegistry::destroy(m_editSessionManager);
    SingletonRegistry::destroy(m_midiExtractController);
    SingletonRegistry::destroy(m_pitchExtractController);
    SingletonRegistry::destroy(m_undoRedoController);
    SingletonRegistry::destroy(m_editorViewController);
    SingletonRegistry::destroy(m_clipController);
    SingletonRegistry::destroy(m_trackController);
    SingletonRegistry::destroy(m_clipboardController);
    SingletonRegistry::destroy(m_audioDecodingController);
    SingletonRegistry::destroy(m_levelMeterManager);
    SingletonRegistry::destroy(m_historyManager);
    SingletonRegistry::destroy(m_paramUtils);
    SingletonRegistry::destroy(m_appModel);
    SingletonRegistry::destroy(m_appStatus);
    unregisterServices();
    AppContext::activateSession(nullptr);
}

QUuid DocumentSession::id() const {
    return m_id;
}

void DocumentSession::activate() {
    AppContext::activateSession(this);
}

DocumentWorkflowController *DocumentSession::workflow() const {
    return m_documentWorkflowController;
}

PlaybackController *DocumentSession::playback() const {
    return m_playbackController;
}

void DocumentSession::registerServices() {
    if (!m_appStatus)
        return;
    SingletonRegistry::add(m_appStatus);
    SingletonRegistry::add(m_appModel);
    SingletonRegistry::add(m_paramUtils);
    SingletonRegistry::add(m_historyManager);
    SingletonRegistry::add(m_levelMeterManager);
    SingletonRegistry::add(m_audioDecodingController);
    SingletonRegistry::add(m_clipboardController);
    SingletonRegistry::add(m_trackController);
    SingletonRegistry::add(m_clipController);
    SingletonRegistry::add(m_editorViewController);
    SingletonRegistry::add(m_undoRedoController);
    SingletonRegistry::add(m_pitchExtractController);
    SingletonRegistry::add(m_midiExtractController);
    SingletonRegistry::add(m_editSessionManager);
    SingletonRegistry::add(m_playbackController);
    SingletonRegistry::add(m_projectStatusController);
    SingletonRegistry::add(m_projectPackageResolver);
    SingletonRegistry::add(m_inferController);
    SingletonRegistry::add(m_documentImportController);
    SingletonRegistry::add(m_audioContext.get());
    SingletonRegistry::add(m_appController);
    SingletonRegistry::add(m_documentWorkflowController);
}

void DocumentSession::unregisterServices() {
    SingletonRegistry::remove<DocumentWorkflowController>();
    SingletonRegistry::remove<AppController>();
    SingletonRegistry::remove<AudioContext>();
    SingletonRegistry::remove<DocumentImportController>();
    SingletonRegistry::remove<InferController>();
    SingletonRegistry::remove<ProjectPackageResolver>();
    SingletonRegistry::remove<ProjectStatusController>();
    SingletonRegistry::remove<PlaybackController>();
    SingletonRegistry::remove<EditSessionManager>();
    SingletonRegistry::remove<MidiExtractController>();
    SingletonRegistry::remove<PitchExtractController>();
    SingletonRegistry::remove<UndoRedoController>();
    SingletonRegistry::remove<EditorViewController>();
    SingletonRegistry::remove<ClipController>();
    SingletonRegistry::remove<TrackController>();
    SingletonRegistry::remove<ClipboardController>();
    SingletonRegistry::remove<AudioDecodingController>();
    SingletonRegistry::remove<LevelMeterManager>();
    SingletonRegistry::remove<HistoryManager>();
    SingletonRegistry::remove<ParamUtils>();
    SingletonRegistry::remove<AppModel>();
    SingletonRegistry::remove<AppStatus>();
}
