#include "Automation/AutomationDispatcher.h"
#include "Automation/CoreRuntime.h"
#include "Automation/ProjectAutomationDtos.h"
#include "OperationManifest.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <algorithm>
#include <functional>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    class FakeResolver final : public Automation::IDocumentSessionResolver {
    public:
        FakeResolver(Automation::DocumentSession &first, Automation::DocumentSession &second)
            : m_first(first), m_second(second) {
        }

        Automation::AutomationResult<std::reference_wrapper<Automation::DocumentSession>>
            resolveDocument(const Automation::DocumentId &documentId) override {
            if (documentId == m_first.documentId())
                return std::ref(m_first);
            if (documentId == m_second.documentId())
                return std::ref(m_second);
            return Automation::AutomationError::documentChanged(documentId, m_first.documentId());
        }

    private:
        Automation::DocumentSession &m_first;
        Automation::DocumentSession &m_second;
    };

    Automation::OperationDescriptor commandDescriptor() {
        return {
            .id = QStringLiteral("test.command"),
            .category = QStringLiteral("test"),
            .kind = Automation::OperationKind::Command,
            .syncMode = Automation::SyncMode::Synchronous,
            .inputContract = QStringLiteral("test.CommandInput.v1"),
            .outputContract = QStringLiteral("automation.MutationResult.v1"),
            .documentPolicy = Automation::DocumentPolicy::Write,
            .revisionPolicy = Automation::RevisionPolicy::Increment,
            .historyPolicy = Automation::HistoryPolicy::Record,
            .fileAccess = Automation::FileAccessPolicy::None,
            .hostAvailability = Automation::HostAvailability::Core,
            .safety = Automation::SafetyClass::Reversible,
            .exposure = Automation::ExposurePolicy::InternalOnly,
            .idempotency = Automation::IdempotencyPolicy::DocumentGeneration,
        };
    }

    Automation::OperationDescriptor queryDescriptor() {
        return {
            .id = QStringLiteral("test.query"),
            .category = QStringLiteral("test"),
            .kind = Automation::OperationKind::Query,
            .syncMode = Automation::SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.DocumentRef.v1"),
            .outputContract = QStringLiteral("test.Revision.v1"),
            .documentPolicy = Automation::DocumentPolicy::Read,
            .revisionPolicy = Automation::RevisionPolicy::None,
            .historyPolicy = Automation::HistoryPolicy::None,
            .fileAccess = Automation::FileAccessPolicy::None,
            .hostAvailability = Automation::HostAvailability::Core,
            .safety = Automation::SafetyClass::ReadOnly,
            .exposure = Automation::ExposurePolicy::InternalOnly,
            .idempotency = Automation::IdempotencyPolicy::Unsupported,
        };
    }

    bool hasTempoAt(const AppModel &model, const int tick, const double value) {
        const auto &tempos = model.timeline().tempos();
        return std::any_of(tempos.cbegin(), tempos.cend(), [tick, value](const Tempo &tempo) {
            return tempo.pos == tick && tempo.value == value;
        });
    }

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime,
                                              const bool validateOnly = false) {
        return {.expected = runtime.documentVersion(),
                .validateOnly = validateOnly,
                .source = Automation::InvocationSource::Test};
    }

    struct FakeAudioExportState {
        int executeCount = 0;
        int cleanupCount = 0;
        bool canceled = false;
        Automation::AudioExportBackendState result = Automation::AudioExportBackendState::Succeeded;
    };

    class FakeAudioExportJob final : public Automation::IAudioExportJob {
    public:
        FakeAudioExportJob(Automation::AudioExportConfigDto config,
                           std::shared_ptr<FakeAudioExportState> state)
            : m_config(std::move(config)), m_state(std::move(state)) {
        }

        Automation::AudioExportPreviewDto preview() const override {
            return {
                .baseDirectory = m_config.fileDirectory,
                .filePaths = {QDir(m_config.fileDirectory).absoluteFilePath(m_config.fileName)},
                .warningFlags = m_config.fileType >= 2 ? Automation::AudioExportLossyFormat : 0,
            };
        }

        Automation::AudioExportBackendResult
            execute(const Automation::AudioExportObserver &observer) override {
            ++m_state->executeCount;
            if (observer.progress)
                observer.progress(0.5, -1);
            if (m_state->canceled)
                return {.state = Automation::AudioExportBackendState::Canceled};
            return {.state = m_state->result,
                    .errorMessage = m_state->result == Automation::AudioExportBackendState::Failed
                                        ? QStringLiteral("simulated audio export failure")
                                        : QString()};
        }

        void cancel() override {
            m_state->canceled = true;
        }

        void cleanup() override {
            ++m_state->cleanupCount;
        }

    private:
        Automation::AudioExportConfigDto m_config;
        std::shared_ptr<FakeAudioExportState> m_state;
    };

    struct FakePitchExtractionState {
        int startCount = 0;
        int cancelCount = 0;
        bool canceled = false;
        Automation::ExtractionJobCallbacks callbacks;
        std::function<void(Automation::PitchExtractionBackendResult)> completed;
    };

    class FakePitchExtractionJob final : public Automation::IPitchExtractionJob {
    public:
        explicit FakePitchExtractionJob(std::shared_ptr<FakePitchExtractionState> state)
            : m_state(std::move(state)) {
        }

        void start(
            Automation::ExtractionJobCallbacks callbacks,
            std::function<void(Automation::PitchExtractionBackendResult)> completed) override {
            ++m_state->startCount;
            m_state->callbacks = std::move(callbacks);
            m_state->completed = std::move(completed);
            if (m_state->callbacks.progress) {
                m_state->callbacks.progress(
                    {.minimum = 0, .maximum = 100, .value = 25, .indeterminate = false},
                    QStringLiteral("extracting pitch"));
            }
        }

        void cancel() override {
            if (m_state->canceled)
                return;
            m_state->canceled = true;
            ++m_state->cancelCount;
        }

    private:
        std::shared_ptr<FakePitchExtractionState> m_state;
    };

    struct FakeMidiExtractionState {
        int startCount = 0;
        int cancelCount = 0;
        bool canceled = false;
        Automation::ExtractionJobCallbacks callbacks;
        std::function<void(Automation::MidiExtractionBackendResult)> completed;
    };

    class FakeMidiExtractionJob final : public Automation::IMidiExtractionJob {
    public:
        explicit FakeMidiExtractionJob(std::shared_ptr<FakeMidiExtractionState> state)
            : m_state(std::move(state)) {
        }

        void
            start(Automation::ExtractionJobCallbacks callbacks,
                  std::function<void(Automation::MidiExtractionBackendResult)> completed) override {
            ++m_state->startCount;
            m_state->callbacks = std::move(callbacks);
            m_state->completed = std::move(completed);
            if (m_state->callbacks.progress) {
                m_state->callbacks.progress(
                    {.minimum = 0, .maximum = 100, .value = 50, .indeterminate = false},
                    QStringLiteral("extracting MIDI"));
            }
        }

        void cancel() override {
            if (m_state->canceled)
                return;
            m_state->canceled = true;
            ++m_state->cancelCount;
        }

    private:
        std::shared_ptr<FakeMidiExtractionState> m_state;
    };
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = true;

    Automation::DocumentSession first(nullptr, nullptr);
    Automation::DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    Automation::SingleWindowContext window;
    Automation::OperationCatalog catalog;

    ok &= expect(catalog.add(queryDescriptor()).isPresent(), "query descriptor must register");
    ok &= expect(catalog.add(commandDescriptor()).isPresent(), "command descriptor must register");
    ok &= expect(!catalog.add(commandDescriptor()).isPresent(),
                 "duplicate operation ID must be rejected");
    auto otherCommand = commandDescriptor();
    otherCommand.id = QStringLiteral("test.other_command");
    ok &= expect(catalog.add(std::move(otherCommand)).isPresent(),
                 "second command descriptor must register");
    ok &=
        expect(Automation::errorCodeName(Automation::AutomationErrorCode::PathRequired) ==
                       QStringLiteral("path_required") &&
                   Automation::errorCodeName(Automation::AutomationErrorCode::FileNotFound) ==
                       QStringLiteral("file_not_found") &&
                   Automation::errorCodeName(Automation::AutomationErrorCode::FormatUnsupported) ==
                       QStringLiteral("format_unsupported") &&
                   Automation::errorCodeName(Automation::AutomationErrorCode::OverwriteDenied) ==
                       QStringLiteral("overwrite_denied") &&
                   Automation::errorCodeName(Automation::AutomationErrorCode::InferenceError) ==
                       QStringLiteral("inference_error") &&
                   Automation::errorCodeName(Automation::AutomationErrorCode::Unsupported) ==
                       QStringLiteral("unsupported"),
               "file error codes must keep stable external names");

    Automation::AutomationDispatcher dispatcher(resolver, window, catalog);
    auto secondQuery = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), second.documentId(),
        [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &= expect(secondQuery && secondQuery.get() == 0,
                 "dispatcher must route by explicit document ID");

    Automation::CommandContext context;
    context.expected = first.version();
    context.idempotencyKey = QStringLiteral("8ff6e1d7-a1d7-463d-9a6b-f85913fe0773");
    int executionCount = 0;
    const auto handler = [&executionCount](Automation::DocumentSession &session,
                                           const bool validateOnly) {
        ++executionCount;
        Automation::MutationResult result;
        result.previous = session.version();
        result.changed = true;
        result.validatedOnly = validateOnly;
        result.current = validateOnly ? session.version() : session.advanceRevision();
        return Automation::AutomationResult<Automation::MutationResult>(result);
    };

    const auto firstResult = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), context, QByteArrayLiteral("payload"), handler);
    ok &= expect(firstResult && firstResult.get().current.revision == 1,
                 "command must return the committed revision");

    const auto replayed = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), context, QByteArrayLiteral("payload"), handler);
    ok &= expect(replayed && replayed.get() == firstResult.get(),
                 "same idempotency key must replay the original result");
    ok &= expect(executionCount == 1 && first.revision() == 1,
                 "idempotent replay must not execute or increment revision again");

    const auto conflict = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), context, QByteArrayLiteral("different"), handler);
    ok &= expect(!conflict && conflict.getError().code ==
                                  Automation::AutomationErrorCode::IdempotencyConflict,
                 "same key with another request must fail with idempotency conflict");
    const auto operationConflict = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.other_command"), context, QByteArrayLiteral("payload"), handler);
    ok &= expect(!operationConflict && operationConflict.getError().code ==
                                           Automation::AutomationErrorCode::IdempotencyConflict,
                 "the same idempotency key cannot be reused by another operation");

    const auto oldDocumentId = first.documentId();
    first.replaceGeneration({}, QStringLiteral("Replacement"));
    ok &= expect(first.idempotencyStore().size() == 0,
                 "replacing the generation must clear idempotency records");
    const auto stale = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), oldDocumentId, [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &=
        expect(!stale && stale.getError().code == Automation::AutomationErrorCode::DocumentChanged,
               "old document ID must fail after generation replacement");

    const auto invalidWindow = window.validateWindow(Automation::WindowId::create());
    ok &= expect(!invalidWindow && invalidWindow.getError().code ==
                                       Automation::AutomationErrorCode::HostCapabilityUnavailable,
                 "single-window host must reject another window ID");

    AppModel model;
    auto *history = HistoryManager::instance();
    history->reset();
    int saveCount = 0;
    LoopSettings appliedLoopSettings;
    Automation::DocumentRuntimeServices documentServices;
    documentServices.applyLoopSettings = [&appliedLoopSettings](const LoopSettings &settings) {
        appliedLoopSettings = settings;
    };
    documentServices.saveProject = [&saveCount](const QString &, AppModel *, QString &) {
        ++saveCount;
        return true;
    };
    Automation::PlaybackHostSnapshot playbackHost;
    bool playbackCanStart = true;
    Automation::PlaybackRuntimeServices playbackServices;
    playbackServices.snapshot = [&playbackHost] { return playbackHost; };
    playbackServices.canStart = [&playbackCanStart] { return playbackCanStart; };
    playbackServices.play = [&playbackHost] {
        playbackHost.state = Automation::PlaybackState::Playing;
        return true;
    };
    playbackServices.pause = [&playbackHost] {
        playbackHost.state = Automation::PlaybackState::Paused;
    };
    playbackServices.stop = [&playbackHost] {
        playbackHost.state = Automation::PlaybackState::Stopped;
    };
    playbackServices.setPosition = [&playbackHost](const double tick) {
        playbackHost.position = tick;
    };
    playbackServices.setLastPosition = [&playbackHost](const double tick) {
        playbackHost.lastPosition = tick;
    };
    playbackServices.setLoop = [&playbackHost](const LoopSettings &settings) {
        playbackHost.loop = settings;
    };
    EditorViewState editorViewState;
    Automation::EditorStableState editorStableState;
    bool editorRevealApplied = false;
    Automation::EditorRuntimeServices editorServices;
    editorServices.captureView = [&editorViewState] {
        return std::optional<EditorViewState>(editorViewState);
    };
    editorServices.captureStableState = [&editorStableState] { return editorStableState; };
    editorServices.restoreView = [&editorViewState](const EditorViewState &state) {
        editorViewState = state;
        return true;
    };
    editorServices.centerTrackPanel = [&editorViewState](const double tick,
                                                         const double trackIndex) {
        editorViewState.trackPanel.centerTick = tick;
        editorViewState.trackPanel.centerTrackIndex = trackIndex;
        return true;
    };
    editorServices.setTrackPanelScale = [&editorViewState](const double horizontal,
                                                           const double vertical) {
        editorViewState.trackPanel.horizontalScale = horizontal;
        editorViewState.trackPanel.verticalScale = vertical;
        return true;
    };
    editorServices.setPanelVisibility = [&editorViewState](const bool trackVisible,
                                                           const bool bottomVisible) {
        editorViewState.layout.trackPanelVisible = trackVisible;
        editorViewState.layout.bottomPanelVisible = bottomVisible;
        return true;
    };
    editorServices.showBottomPanelPage = [&editorViewState](const QString &pageId) {
        editorViewState.layout.bottomPanelPageId = pageId;
        return true;
    };
    editorServices.centerPianoRoll = [&editorViewState](const double tick, const double keyIndex) {
        editorViewState.pianoRoll.centerTick = tick;
        editorViewState.pianoRoll.centerKeyIndex = keyIndex;
        return true;
    };
    editorServices.setPianoRollScale = [&editorViewState](const double horizontal,
                                                          const double vertical) {
        editorViewState.pianoRoll.horizontalScale = horizontal;
        editorViewState.pianoRoll.verticalScale = vertical;
        return true;
    };
    editorServices.setPianoRollEditMode = [&editorViewState](const auto mode) {
        editorViewState.pianoRoll.editMode = mode;
        return true;
    };
    editorServices.setActiveClip = [&editorStableState](const int clipId) {
        if (editorStableState.activeClipId != clipId)
            editorStableState.selectedNoteIds.clear();
        editorStableState.activeClipId = clipId;
    };
    editorServices.setSelectedTrackIndex = [&editorStableState](const int index) {
        editorStableState.selectedTrackIndex = index;
    };
    editorServices.setSelectedClips = [&editorStableState](const QList<int> &ids) {
        editorStableState.selectedClipIds = ids;
    };
    editorServices.setSelectedNotes = [&editorStableState](const int clipId,
                                                           const QList<int> &ids) {
        editorStableState.activeClipId = clipId;
        editorStableState.selectedNoteIds = ids;
    };
    editorServices.setPianoRollQuantize = [&editorStableState](const int quantize,
                                                               const bool enabled) {
        editorStableState.pianoRollQuantize = quantize;
        editorStableState.pianoRollQuantizeEnabled = enabled;
    };
    editorServices.setAutoPageTurn = [&editorStableState](const auto target, const bool enabled) {
        if (target == Automation::EditorAutoPageTarget::TrackPanel)
            editorStableState.trackAutoPageTurnEnabled = enabled;
        else
            editorStableState.pianoRollAutoPageTurnEnabled = enabled;
    };
    editorServices.revealFocus = [&editorRevealApplied](const HistoryFocus &, const bool) {
        editorRevealApplied = true;
        return true;
    };
    Automation::SettingsSnapshotDto applicationSettings;
    applicationSettings.general.uiLanguage = QStringLiteral("system");
    applicationSettings.general.defaultSingingLanguage = QStringLiteral("cmn");
    applicationSettings.general.defaultLyrics.insert(QStringLiteral("cmn"), QStringLiteral("啦"));
    applicationSettings.appearance.themeId = QStringLiteral("system");
    applicationSettings.inference.executionProvider = QStringLiteral("CPU");
    applicationSettings.inference.cacheDirectory = QStringLiteral("cache");
    for (int index = 0; index < 4; ++index) {
        applicationSettings.audio.pseudoSingerSynthesizers.append({
            .generator = index,
            .amplitude = -12.0,
            .attackMilliseconds = 50,
            .decayMilliseconds = 1000,
            .decayRatio = 0.5,
            .releaseMilliseconds = 50,
        });
    }
    int settingsWriteCount = 0;
    Automation::SettingsRuntimeServices settingsServices;
    settingsServices.snapshot = [&applicationSettings] { return applicationSettings; };
    settingsServices.applyGeneral = [&applicationSettings, &settingsWriteCount](const auto &value) {
        applicationSettings.general = value;
        ++settingsWriteCount;
        return true;
    };
    settingsServices.applyAppearance = [&applicationSettings,
                                        &settingsWriteCount](const auto &value) {
        applicationSettings.appearance = value;
        ++settingsWriteCount;
        return true;
    };
    settingsServices.applyInference = [&applicationSettings,
                                       &settingsWriteCount](const auto &value) {
        applicationSettings.inference = value;
        ++settingsWriteCount;
        return true;
    };
    settingsServices.applyDeveloper = [&applicationSettings,
                                       &settingsWriteCount](const auto &value) {
        applicationSettings.developer = value;
        ++settingsWriteCount;
        return true;
    };
    settingsServices.applyG2pLanguage = [&applicationSettings,
                                         &settingsWriteCount](const auto &value) {
        applicationSettings.g2pLanguage = value;
        ++settingsWriteCount;
        return true;
    };
    settingsServices.applyFillLyric = [&applicationSettings,
                                       &settingsWriteCount](const auto &value) {
        applicationSettings.fillLyric = value;
        ++settingsWriteCount;
        return true;
    };
    settingsServices.applyWindow = [&applicationSettings, &settingsWriteCount](const auto &value) {
        applicationSettings.window = value;
        ++settingsWriteCount;
        return true;
    };
    settingsServices.applyAudio = [&applicationSettings, &settingsWriteCount](const auto &value) {
        applicationSettings.audio = value;
        ++settingsWriteCount;
        return true;
    };
    QList<Automation::SpeakerMixPresetDto> speakerMixPresets;
    int presetWriteCount = 0;
    Automation::PresetRuntimeServices presetServices;
    presetServices.speakerMixPresets = [&speakerMixPresets] { return speakerMixPresets; };
    presetServices.applySpeakerMixPresets = [&speakerMixPresets,
                                             &presetWriteCount](const auto &value) {
        speakerMixPresets = value;
        ++presetWriteCount;
        return true;
    };
    Automation::PackageRuntimeServices packageServices;
    packageServices.installedPackages = [] {
        return QList<Automation::PackageDto>{
            {
             .id = QStringLiteral("voice.package"),
             .version = QVersionNumber(1, 0),
             .path = QStringLiteral("packages/voice.package"),
             }
        };
    };
    packageServices.validatePackage = [](const QString &) {
        return Automation::AutomationResult<Automation::PackageValidationReportDto>(
            Automation::PackageValidationReportDto{});
    };
    int packageResolveApplyCount = 0;
    packageServices.resolveDocumentVoices = [&packageResolveApplyCount](AppModel *,
                                                                        const bool apply) {
        if (apply)
            ++packageResolveApplyCount;
        return 1;
    };
    int inferenceApplyCount = 0;
    Automation::InferenceRuntimeServices inferenceServices;
    inferenceServices.prepareMutation =
        [&inferenceApplyCount](AppModel *, const Automation::InferenceMutationRequest &request) {
            Automation::PreparedInferenceMutation prepared;
            prepared.changed = true;
            prepared.advancesRevision =
                request.kind != Automation::InferenceMutationKind::ApplyAcoustic;
            prepared.affectedObjects.append({Automation::ObjectKind::Clip, 1});
            prepared.apply = [&inferenceApplyCount](Automation::InferenceMutationSideEffects &) {
                ++inferenceApplyCount;
            };
            return Automation::AutomationResult<Automation::PreparedInferenceMutation>(
                std::move(prepared));
        };
    int midiExportCount = 0;
    Automation::FileRuntimeServices fileServices;
    fileServices.listProjectFormats = [] {
        return QList<Automation::ProjectFormatDto>{
            {.id = QStringLiteral("midi"),
             .displayName = QStringLiteral("MIDI"),
             .extensions = {QStringLiteral("mid"), QStringLiteral("midi")},
             .canOpen = true,
             .canImport = true,
             .canExport = true},
        };
    };
    fileServices.exportMidi = [&midiExportCount](AppModel *, const QString &path,
                                                 QString &errorMessage) {
        ++midiExportCount;
        if (path.endsWith(QStringLiteral("fail.mid"))) {
            errorMessage = QStringLiteral("simulated export failure");
            return false;
        }
        return true;
    };
    auto audioExportState = std::make_shared<FakeAudioExportState>();
    std::function<void()> scheduledAudioExport;
    Automation::AudioExportRuntimeServices audioExportServices;
    audioExportServices.createJob =
        [&audioExportState](AppModel *, const QString &,
                            const Automation::AudioExportConfigDto &config) {
            return Automation::AutomationResult<std::shared_ptr<Automation::IAudioExportJob>>(
                std::make_shared<FakeAudioExportJob>(config, audioExportState));
        };
    audioExportServices.schedule = [&scheduledAudioExport](std::function<void()> execute) {
        scheduledAudioExport = std::move(execute);
    };
    QList<std::shared_ptr<FakePitchExtractionState>> pitchExtractionStates;
    QList<std::shared_ptr<FakeMidiExtractionState>> midiExtractionStates;
    QList<std::function<void()>> scheduledExtractions;
    Automation::ExtractionRuntimeServices extractionServices;
    extractionServices.preparePitch =
        [&pitchExtractionStates](Automation::PitchExtractionInput input) {
            input.modelPath = QStringLiteral("fake-rmvpe.onnx");
            auto state = std::make_shared<FakePitchExtractionState>();
            pitchExtractionStates.append(state);
            return Automation::AutomationResult<Automation::PreparedPitchExtraction>(
                {std::move(input), std::make_shared<FakePitchExtractionJob>(std::move(state))});
        };
    extractionServices.prepareMidi =
        [&midiExtractionStates](Automation::MidiExtractionInput input) {
            input.modelPath = QStringLiteral("fake-game");
            input.defaultLanguage = QStringLiteral("cmn");
            input.defaultLyric = QStringLiteral("啦");
            auto state = std::make_shared<FakeMidiExtractionState>();
            midiExtractionStates.append(state);
            return Automation::AutomationResult<Automation::PreparedMidiExtraction>(
                {std::move(input), std::make_shared<FakeMidiExtractionJob>(std::move(state))});
        };
    extractionServices.schedule = [&scheduledExtractions](std::function<void()> execute) {
        scheduledExtractions.append(std::move(execute));
    };
    int terminationRequestCount = 0;
    Automation::ApplicationTerminationMode lastTerminationMode =
        Automation::ApplicationTerminationMode::Exit;
    Automation::ApplicationRuntimeServices applicationServices;
    applicationServices.info = [] {
        return Automation::ApplicationInfoDto{
            .name = QStringLiteral("DS Editor Lite"),
            .version = QStringLiteral("test"),
            .platform = QStringLiteral("test"),
        };
    };
    applicationServices.requestTermination = [&terminationRequestCount,
                                              &lastTerminationMode](const auto mode) {
        ++terminationRequestCount;
        lastTerminationMode = mode;
        return true;
    };
    Automation::CoreRuntime runtime(&model, history, std::move(documentServices),
                                    std::move(playbackServices), std::move(editorServices),
                                    std::move(settingsServices), std::move(presetServices),
                                    std::move(packageServices), std::move(inferenceServices),
                                    std::move(fileServices), std::move(audioExportServices),
                                    std::move(extractionServices), std::move(applicationServices));
    const auto state =
        runtime.facade().getEditorState(runtime.documentVersion().documentId, runtime.windowId());
    const auto capabilities = runtime.facade().getEditorCapabilities();
    ok &= expect(state && state.get().document == runtime.documentVersion(),
                 "editor state must include the current document version");
    const auto wrongDocumentState =
        runtime.facade().getEditorState(Automation::DocumentId::create(), runtime.windowId());
    const auto wrongWindowState = runtime.facade().getEditorState(
        runtime.documentVersion().documentId, Automation::WindowId::create());
    ok &= expect(!wrongDocumentState &&
                     wrongDocumentState.getError().code ==
                         Automation::AutomationErrorCode::DocumentChanged &&
                     !wrongWindowState &&
                     wrongWindowState.getError().code ==
                         Automation::AutomationErrorCode::HostCapabilityUnavailable,
                 "editor state queries must route by explicit document and window IDs");
    ok &= expect(capabilities && capabilities.get().maxConcurrentDocuments == 1 &&
                     capabilities.get().maxConcurrentWindows == 1,
                 "capabilities must declare the single document/window boundary");
    ok &= expect(capabilities && capabilities.get().operationIds == automationOperationManifest(),
                 "Catalog and the exact operation test manifest must match");

    const auto formats = runtime.files().listFormats();
    ok &= expect(formats && formats.get().size() == 1 && formats.get().first().canExport,
                 "format discovery must return typed handler capabilities");
    QTemporaryDir exportDirectory;
    auto midiContext = commandContext(runtime);
    midiContext.idempotencyKey = QStringLiteral("a6a762a9-ed3c-4bbf-b2a4-921f332cd303");
    const auto midiPath = exportDirectory.filePath(QStringLiteral("automation.mid"));
    auto midiPreviewContext = midiContext;
    midiPreviewContext.validateOnly = true;
    const auto midiPreview = runtime.files().exportMidi(midiPreviewContext, midiPath, false);
    const auto midiExport = runtime.files().exportMidi(midiContext, midiPath, false);
    const auto midiReplay = runtime.files().exportMidi(midiContext, midiPath, false);
    ok &= expect(exportDirectory.isValid() && midiPreview && midiPreview.get().validatedOnly &&
                     !midiPreview.get().wroteFile && midiExport && midiReplay &&
                     midiExport.get() == midiReplay.get() && midiExportCount == 1,
                 "MIDI export must preview without writing and replay idempotently");
    const auto midiConflict = runtime.files().exportMidi(
        midiContext, exportDirectory.filePath(QStringLiteral("another.mid")), false);
    const auto missingMidiPath = runtime.files().exportMidi(commandContext(runtime), {}, false);
    const auto relativeMidiPath =
        runtime.files().exportMidi(commandContext(runtime), QStringLiteral("relative.mid"), false);
    const auto unsupportedMidiPath = runtime.files().exportMidi(
        commandContext(runtime), exportDirectory.filePath(QStringLiteral("automation.wav")), false);
    const auto absentMidiDirectory = runtime.files().exportMidi(
        commandContext(runtime), exportDirectory.filePath(QStringLiteral("absent/automation.mid")),
        false);
    ok &= expect(
        !midiConflict &&
            midiConflict.getError().code == Automation::AutomationErrorCode::IdempotencyConflict &&
            !missingMidiPath &&
            missingMidiPath.getError().code == Automation::AutomationErrorCode::PathRequired &&
            !relativeMidiPath &&
            relativeMidiPath.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
            !unsupportedMidiPath &&
            unsupportedMidiPath.getError().code ==
                Automation::AutomationErrorCode::FormatUnsupported &&
            !absentMidiDirectory &&
            absentMidiDirectory.getError().code == Automation::AutomationErrorCode::FileNotFound,
        "MIDI export must expose stable idempotency and path validation errors");
    const auto existingMidiPath = exportDirectory.filePath(QStringLiteral("existing.mid"));
    QFile existingMidi(existingMidiPath);
    const auto createdExistingMidi = existingMidi.open(QIODevice::WriteOnly);
    existingMidi.close();
    const auto overwriteDenied =
        runtime.files().exportMidi(commandContext(runtime), existingMidiPath, false);
    const auto failedMidiExport = runtime.files().exportMidi(
        commandContext(runtime), exportDirectory.filePath(QStringLiteral("fail.mid")), false);
    ok &= expect(createdExistingMidi && !overwriteDenied &&
                     overwriteDenied.getError().code ==
                         Automation::AutomationErrorCode::OverwriteDenied &&
                     !failedMidiExport &&
                     failedMidiExport.getError().code == Automation::AutomationErrorCode::IoError &&
                     midiExportCount == 2,
                 "MIDI export must enforce overwrite policy and preserve backend failures");

    Automation::AudioExportConfigDto audioExportConfig;
    audioExportConfig.fileName = QStringLiteral("automation.wav");
    audioExportConfig.fileDirectory = exportDirectory.path();
    const auto audioPreview =
        runtime.audioExports().preview(runtime.documentVersion().documentId, audioExportConfig);
    auto audioValidateContext = commandContext(runtime, true);
    audioValidateContext.idempotencyKey = QStringLiteral("12d0198d-57d7-4454-84c8-19fdce34e457");
    const auto audioValidation =
        runtime.audioExports().start(audioValidateContext, audioExportConfig, {});
    auto audioContext = commandContext(runtime);
    audioContext.idempotencyKey = audioValidateContext.idempotencyKey;
    const auto audioAccepted = runtime.audioExports().start(audioContext, audioExportConfig, {});
    const auto audioReplayed = runtime.audioExports().start(audioContext, audioExportConfig, {});
    ok &=
        expect(audioPreview && audioPreview.get().filePaths.size() == 1 && audioValidation &&
                   audioValidation.get().validatedOnly && audioValidation.get().taskId.isNull() &&
                   audioAccepted && audioReplayed && audioAccepted.get() == audioReplayed.get() &&
                   scheduledAudioExport,
               "audio export must preview, validate without a task, and replay one accepted task");
    const auto audioVersion = runtime.documentVersion();
    scheduledAudioExport();
    scheduledAudioExport = {};
    const auto completedAudioTask =
        runtime.tasks().getTask(runtime.documentVersion().documentId, audioAccepted.get().taskId);
    const auto cleanedAudioTask =
        runtime.audioExports().cleanup(commandContext(runtime), audioAccepted.get().taskId);
    const auto repeatedAudioCleanup =
        runtime.audioExports().cleanup(commandContext(runtime), audioAccepted.get().taskId);
    ok &= expect(completedAudioTask &&
                     completedAudioTask.get().state == Automation::AutomationTaskState::Succeeded &&
                     completedAudioTask.get().progress.value == 50 &&
                     audioExportState->executeCount == 1 &&
                     runtime.documentVersion() == audioVersion && cleanedAudioTask &&
                     cleanedAudioTask.get().changed && repeatedAudioCleanup &&
                     !repeatedAudioCleanup.get().changed && audioExportState->cleanupCount == 1,
                 "audio export must retain task progress, preserve revision, and clean up once");

    auto unsupportedAudioConfig = audioExportConfig;
    unsupportedAudioConfig.sourceOption = 1;
    const auto unsupportedAudio = runtime.audioExports().preview(
        runtime.documentVersion().documentId, unsupportedAudioConfig);
    auto mismatchedAudioConfig = audioExportConfig;
    mismatchedAudioConfig.fileName = QStringLiteral("automation.mp3");
    const auto mismatchedAudio =
        runtime.audioExports().preview(runtime.documentVersion().documentId, mismatchedAudioConfig);
    auto escapingAudioConfig = audioExportConfig;
    escapingAudioConfig.fileName = QStringLiteral("../escaping.wav");
    const auto escapingAudio =
        runtime.audioExports().start(commandContext(runtime), escapingAudioConfig, {});
    auto lossyAudioConfig = audioExportConfig;
    lossyAudioConfig.fileName = QStringLiteral("automation.ogg");
    lossyAudioConfig.fileType = 2;
    const auto rejectedLossyAudio =
        runtime.audioExports().start(commandContext(runtime), lossyAudioConfig, {});
    ok &= expect(
        !unsupportedAudio &&
            unsupportedAudio.getError().code == Automation::AutomationErrorCode::Unsupported &&
            !mismatchedAudio &&
            mismatchedAudio.getError().code == Automation::AutomationErrorCode::FormatUnsupported &&
            !escapingAudio &&
            escapingAudio.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
            !rejectedLossyAudio &&
            rejectedLossyAudio.getError().code == Automation::AutomationErrorCode::InvalidArgument,
        "audio export must reject deferred modes, unsafe paths, and implicit warnings");

    auto canceledAudioConfig = audioExportConfig;
    canceledAudioConfig.fileName = QStringLiteral("canceled.wav");
    const auto canceledAudioAccepted =
        runtime.audioExports().start(commandContext(runtime), canceledAudioConfig, {});
    const auto audioCancel =
        runtime.tasks().cancelTask(commandContext(runtime), canceledAudioAccepted.get().taskId);
    scheduledAudioExport();
    scheduledAudioExport = {};
    const auto canceledAudioTask = runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                           canceledAudioAccepted.get().taskId);
    ok &= expect(audioCancel && canceledAudioTask &&
                     canceledAudioTask.get().state == Automation::AutomationTaskState::Canceled &&
                     audioExportState->executeCount == 1,
                 "queued audio export cancellation must prevent backend execution");

    audioExportState->result = Automation::AudioExportBackendState::Failed;
    auto failedAudioConfig = audioExportConfig;
    failedAudioConfig.fileName = QStringLiteral("failed.wav");
    const auto failedAudioAccepted =
        runtime.audioExports().start(commandContext(runtime), failedAudioConfig, {});
    scheduledAudioExport();
    scheduledAudioExport = {};
    const auto failedAudioTask = runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                         failedAudioAccepted.get().taskId);
    ok &= expect(
        failedAudioTask && failedAudioTask.get().state == Automation::AutomationTaskState::Failed &&
            failedAudioTask.get().error &&
            failedAudioTask.get().error->code == Automation::AutomationErrorCode::IoError &&
            audioExportState->executeCount == 2,
        "audio export backend failures must remain queryable as stable task errors");
    audioExportState->result = Automation::AudioExportBackendState::Succeeded;

    Automation::InferenceMutationRequest acousticRequest;
    acousticRequest.kind = Automation::InferenceMutationKind::ApplyAcoustic;
    acousticRequest.clipId = Automation::ClipId(1);
    acousticRequest.pieceId = Automation::PieceId(2);
    acousticRequest.acousticPath = QStringLiteral("cache.wav");
    const auto acousticVersion = runtime.documentVersion();
    const auto acousticApply =
        runtime.inference().applyMutation(commandContext(runtime), acousticRequest);
    ok &= expect(acousticApply && acousticApply.get().mutation.changed &&
                     inferenceApplyCount == 1 && runtime.documentVersion() == acousticVersion,
                 "rebuildable inference cache writeback must keep document revision unchanged");

    const auto playbackVersion = runtime.documentVersion();
    const auto playbackPreview =
        runtime.playback().setPosition(commandContext(runtime, true), 960.0);
    const auto playbackPosition = runtime.playback().setPosition(commandContext(runtime), 960.0);
    const auto playbackPlay = runtime.playback().play(commandContext(runtime));
    const auto playbackSnapshot =
        runtime.playback().getPlayback(runtime.documentVersion().documentId);
    ok &= expect(playbackPreview && playbackPreview.get().validatedOnly && playbackPosition &&
                     playbackPlay && playbackSnapshot &&
                     playbackSnapshot.get().state == Automation::PlaybackState::Playing &&
                     playbackSnapshot.get().position == 960.0 &&
                     runtime.documentVersion() == playbackVersion,
                 "playback commands must use explicit document routing without changing revision");
    playbackCanStart = false;
    runtime.playback().stop(commandContext(runtime));
    const auto blockedPlayback = runtime.playback().play(commandContext(runtime));
    ok &= expect(!blockedPlayback &&
                     blockedPlayback.getError().code == Automation::AutomationErrorCode::Busy,
                 "playback start must report an in-progress editor gesture");
    playbackCanStart = true;
    const LoopSettings loopSettings(true, 480, 960);
    const auto loopPreview =
        runtime.playback().setLoop(commandContext(runtime, true), loopSettings);
    const auto loopUpdate = runtime.playback().setLoop(commandContext(runtime), loopSettings);
    const auto loopSnapshot = runtime.playback().getPlayback(runtime.documentVersion().documentId);
    const auto invalidLoop =
        runtime.playback().setLoop(commandContext(runtime), LoopSettings(true, 0, 0));
    auto invalidLoopStaleContext = commandContext(runtime);
    ++invalidLoopStaleContext.expected.revision;
    const auto invalidLoopWithStaleRevision =
        runtime.playback().setLoop(invalidLoopStaleContext, LoopSettings(true, 0, 0));
    const auto loopUndo = runtime.history().undo(commandContext(runtime));
    const auto loopAfterUndo = runtime.playback().getPlayback(runtime.documentVersion().documentId);
    const auto loopRedo = runtime.history().redo(commandContext(runtime));
    const auto loopAfterRedo = runtime.playback().getPlayback(runtime.documentVersion().documentId);
    ok &= expect(
        loopPreview && loopPreview.get().validatedOnly && loopPreview.get().changed && loopUpdate &&
            loopUpdate.get().changed && loopSnapshot && loopSnapshot.get().loop == loopSettings &&
            !invalidLoop &&
            invalidLoop.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
            !invalidLoopWithStaleRevision &&
            invalidLoopWithStaleRevision.getError().code ==
                Automation::AutomationErrorCode::RevisionConflict &&
            loopUndo && loopUndo.get().changed && loopAfterUndo &&
            loopAfterUndo.get().loop == LoopSettings() && loopRedo && loopRedo.get().changed &&
            loopAfterRedo && loopAfterRedo.get().loop == loopSettings &&
            runtime.documentVersion().revision == playbackVersion.revision + 3,
        "persisted loop commands must validate, record history, and advance revision");

    Automation::GuiCommandContext guiContext{
        .windowId = runtime.windowId(),
        .source = Automation::InvocationSource::Test,
    };
    auto guiPreviewContext = guiContext;
    guiPreviewContext.validateOnly = true;
    const auto editorPreview = runtime.facade().centerPianoRoll(guiPreviewContext, 1440.0, 72.0);
    const auto editorCenter = runtime.facade().centerPianoRoll(guiContext, 1440.0, 72.0);
    const auto editorMode =
        runtime.facade().setPianoRollEditMode(guiContext, EditorViewGlobal::DrawNote);
    const auto editorQuantize = runtime.facade().setPianoRollQuantize(guiContext, 24, true);
    const auto editorAutoPage = runtime.facade().setAutoPageTurn(
        guiContext, Automation::EditorAutoPageTarget::PianoRoll, false);
    ok &= expect(editorPreview && editorPreview.get().validatedOnly && editorCenter && editorMode &&
                     editorQuantize && editorAutoPage &&
                     editorViewState.pianoRoll.centerTick == 1440.0 &&
                     editorViewState.pianoRoll.centerKeyIndex == 72.0 &&
                     editorViewState.pianoRoll.editMode == EditorViewGlobal::DrawNote &&
                     editorStableState.pianoRollQuantize == 24 &&
                     !editorStableState.pianoRollAutoPageTurnEnabled,
                 "GUI editor commands must validate and route through the single window context");
    const auto applicationInfo = runtime.application().getInfo();
    auto terminationPreviewContext = guiContext;
    terminationPreviewContext.validateOnly = true;
    const auto terminationPreview = runtime.application().requestTermination(
        terminationPreviewContext, Automation::ApplicationTerminationMode::Restart);
    const auto termination = runtime.application().requestTermination(
        guiContext, Automation::ApplicationTerminationMode::Restart);
    ok &= expect(applicationInfo && applicationInfo.get().version == QStringLiteral("test") &&
                     terminationPreview && terminationPreview.get().validatedOnly && termination &&
                     terminationRequestCount == 1 &&
                     lastTerminationMode == Automation::ApplicationTerminationMode::Restart,
                 "application lifecycle commands must remain host-mediated and validate-only safe");
    guiContext.windowId = Automation::WindowId::create();
    const auto unknownEditorWindow = runtime.facade().setPanelVisibility(guiContext, true, false);
    ok &= expect(!unknownEditorWindow &&
                     unknownEditorWindow.getError().code ==
                         Automation::AutomationErrorCode::HostCapabilityUnavailable,
                 "GUI editor commands must reject an unknown window ID");

    auto generalSettings = applicationSettings.general;
    generalSettings.gameDirectory = QStringLiteral("game");
    Automation::ApplicationCommandContext settingsPreviewContext{
        .validateOnly = true,
        .source = Automation::InvocationSource::Test,
    };
    const auto settingsPreview =
        runtime.settings().updateGeneral(settingsPreviewContext, generalSettings);
    const auto settingsUpdate = runtime.settings().updateGeneral({}, generalSettings);
    const auto settingsNoOp = runtime.settings().updateGeneral({}, generalSettings);
    ok &= expect(settingsPreview && settingsPreview.get().validatedOnly &&
                     settingsPreview.get().changed && settingsUpdate &&
                     settingsUpdate.get().changed && settingsNoOp && !settingsNoOp.get().changed &&
                     settingsWriteCount == 1,
                 "application settings must support validation and no-op persistence");
    auto invalidAppearance = applicationSettings.appearance;
    invalidAppearance.animationTimeScale = 0.0;
    const auto rejectedAppearance = runtime.settings().updateAppearance({}, invalidAppearance);
    ok &= expect(!rejectedAppearance && rejectedAppearance.getError().code ==
                                            Automation::AutomationErrorCode::InvalidArgument,
                 "application settings must reject invalid category values");
    const auto recentAdd = runtime.settings().addRecentProjectFile({}, QStringLiteral("a.dspx"));
    const auto recentDuplicate =
        runtime.settings().addRecentProjectFile({}, QStringLiteral("a.dspx"));
    const auto recentList = runtime.settings().getRecentProjectFiles();
    ok &= expect(recentAdd && recentAdd.get().changed && recentDuplicate &&
                     !recentDuplicate.get().changed && recentList && recentList.get().size() == 1,
                 "recent files must normalize duplicates and persist through settings");
    const auto packagePaths = runtime.settings().setPackageSearchPaths(
        {}, {QStringLiteral("models"), QStringLiteral("models")});
    ok &= expect(packagePaths && packagePaths.get().changed &&
                     applicationSettings.general.packageSearchPaths.size() == 1,
                 "package search paths must normalize duplicate entries");

    Automation::SpeakerMixPresetDto preset{
        .name = QStringLiteral("Lead"),
        .packageId = QStringLiteral("package"),
        .singerId = QStringLiteral("singer"),
        .sources = {{.speakerId = QStringLiteral("speaker")}},
        .fixedWeights = {1.0},
    };
    const auto presetPreview =
        runtime.presets().saveSpeakerMixPreset(settingsPreviewContext, preset);
    const auto presetSave = runtime.presets().saveSpeakerMixPreset({}, preset);
    const auto presetDelete =
        presetSave ? runtime.presets().deleteSpeakerMixPreset({}, presetSave.get().id)
                   : Automation::AutomationResult<Automation::ApplicationMutationResult>(
                         Automation::AutomationError{});
    ok &= expect(presetPreview && presetPreview.get().id.isEmpty() && presetWriteCount == 2 &&
                     presetSave && !presetSave.get().id.isEmpty() && presetDelete &&
                     presetDelete.get().changed && speakerMixPresets.isEmpty(),
                 "speaker mix presets must validate without allocating IDs and persist atomically");
    const auto installedPackages = runtime.packages().getInstalledPackages();
    const auto packageValidation = runtime.packages().validatePackage(QStringLiteral("package"));
    const auto invalidPackageValidation = runtime.packages().validatePackage(QString());
    const auto packageResolveVersion = runtime.documentVersion();
    const auto packageResolvePreview =
        runtime.packages().resolveDocumentVoices(commandContext(runtime, true));
    const auto packageResolve = runtime.packages().resolveDocumentVoices(commandContext(runtime));
    ok &= expect(installedPackages && installedPackages.get().size() == 1 && packageValidation &&
                     !packageValidation.get().hasErrors && !invalidPackageValidation &&
                     invalidPackageValidation.getError().code ==
                         Automation::AutomationErrorCode::InvalidArgument &&
                     packageResolvePreview && packageResolvePreview.get().validatedOnly &&
                     packageResolve && packageResolve.get().changed &&
                     packageResolveApplyCount == 1 &&
                     runtime.documentVersion() == packageResolveVersion,
                 "package operations must expose typed reports and guarded cache resolution");

    const auto timelineBaseVersion = runtime.documentVersion();
    Automation::CommandContext setTempoContext;
    setTempoContext.expected = timelineBaseVersion;
    const auto setTempo = runtime.timeline().setTempo(setTempoContext, 960, 150.0);
    ok &= expect(setTempo && setTempo.get().changed &&
                     setTempo.get().current.revision == timelineBaseVersion.revision + 1 &&
                     hasTempoAt(model, 960, 150.0),
                 "timeline mutation must commit one action and one revision");

    Automation::CommandContext noOpContext;
    noOpContext.expected = runtime.documentVersion();
    const auto noOp = runtime.timeline().setTempo(noOpContext, 960, 150.0);
    ok &= expect(noOp && !noOp.get().changed &&
                     runtime.documentVersion().revision == timelineBaseVersion.revision + 1,
                 "legal no-op must not record history or advance revision");

    Automation::CommandContext validateContext;
    validateContext.expected = runtime.documentVersion();
    validateContext.validateOnly = true;
    const auto preview = runtime.timeline().setTempo(validateContext, 960, 160.0);
    ok &= expect(preview && preview.get().validatedOnly && preview.get().changed &&
                     preview.get().current.revision == timelineBaseVersion.revision + 2 &&
                     runtime.documentVersion().revision == timelineBaseVersion.revision + 1 &&
                     hasTempoAt(model, 960, 150.0),
                 "validate-only must predict the result without changing model or revision");

    Automation::CommandContext staleContext;
    staleContext.expected = timelineBaseVersion;
    const auto staleMutation = runtime.timeline().setTempo(staleContext, 960, 160.0);
    ok &= expect(!staleMutation && staleMutation.getError().code ==
                                       Automation::AutomationErrorCode::RevisionConflict,
                 "revision validation must precede domain mutation");

    const auto historyState = runtime.history().getState(runtime.documentVersion().documentId);
    ok &= expect(historyState && historyState.get().canUndo && !historyState.get().canRedo,
                 "history query must reflect the committed timeline action");

    Automation::CommandContext undoContext;
    undoContext.expected = runtime.documentVersion();
    const auto undo = runtime.history().undo(undoContext);
    ok &= expect(undo && undo.get().changed &&
                     undo.get().current.revision == timelineBaseVersion.revision + 2 &&
                     !hasTempoAt(model, 960, 150.0),
                 "undo must use the same revision-owning commit path");

    Automation::CommandContext redoContext;
    redoContext.expected = runtime.documentVersion();
    const auto redo = runtime.history().redo(redoContext);
    ok &= expect(redo && redo.get().changed &&
                     redo.get().current.revision == timelineBaseVersion.revision + 3 &&
                     hasTempoAt(model, 960, 150.0),
                 "redo must use the same revision-owning commit path");

    Automation::CommandContext emptyRedoContext;
    emptyRedoContext.expected = runtime.documentVersion();
    const auto emptyRedo = runtime.history().redo(emptyRedoContext);
    ok &= expect(emptyRedo && !emptyRedo.get().changed &&
                     runtime.documentVersion().revision == timelineBaseVersion.revision + 3,
                 "empty redo must be a successful no-op");

    history->reset();

    Automation::TrackDraftDto trackDraft;
    trackDraft.clientRef = QStringLiteral("track-main");
    trackDraft.name = QStringLiteral("Automation Track");
    trackDraft.gain = 1.0;
    trackDraft.defaultLanguage = QStringLiteral("unknown");

    const auto trackPreview =
        runtime.project().insertTrack(commandContext(runtime, true), 0, trackDraft);
    ok &=
        expect(trackPreview && trackPreview.get().validatedOnly && trackPreview.get().changed &&
                   trackPreview.get().current.revision == runtime.documentVersion().revision + 1 &&
                   trackPreview.get().createdObjects.isEmpty() && model.tracks().isEmpty(),
               "track validate-only must not allocate IDs or insert a track");

    const auto insertTrack = runtime.project().insertTrack(commandContext(runtime), 0, trackDraft);
    ok &= expect(insertTrack && insertTrack.get().changed &&
                     insertTrack.get().affectedObjects.size() == 1 &&
                     insertTrack.get().createdObjects ==
                         QList<Automation::CreatedObjectRef>{
                             {
                              QStringLiteral("track-main"),
                              insertTrack.get().affectedObjects.first(),
                              }
    } &&
                     model.tracks().size() == 1,
                 "track insertion must commit once and bind client_ref to the new track ID");
    const auto trackId = insertTrack
                             ? Automation::TrackId(insertTrack.get().affectedObjects.first().value)
                             : Automation::TrackId();
    auto *track = model.findTrackById(trackId.value());
    ok &= expect(track && track->name() == trackDraft.name,
                 "inserted track must preserve requested properties");

    Automation::CommandContext wrongDocumentContext = commandContext(runtime);
    wrongDocumentContext.expected = {Automation::DocumentId::create(), 999};
    const auto wrongDocument =
        runtime.project().setTrackColor(wrongDocumentContext, Automation::TrackId(999999), 1);
    ok &= expect(!wrongDocument && wrongDocument.getError().code ==
                                       Automation::AutomationErrorCode::DocumentChanged,
                 "document ID validation must precede revision and object validation");

    Automation::CommandContext staleObjectContext = commandContext(runtime);
    staleObjectContext.expected.revision -= 1;
    const auto staleObject =
        runtime.project().setTrackColor(staleObjectContext, Automation::TrackId(999999), 1);
    ok &= expect(!staleObject && staleObject.getError().code ==
                                     Automation::AutomationErrorCode::RevisionConflict,
                 "revision validation must precede object validation");

    Automation::ClipDraftDto clipDraft;
    clipDraft.clientRef = QStringLiteral("clip-main");
    clipDraft.type = Automation::ClipDraftDto::Type::Singing;
    clipDraft.properties.name = QStringLiteral("Automation Clip");
    clipDraft.properties.start = 0;
    clipDraft.properties.length = 1920;
    clipDraft.properties.clipStart = 0;
    clipDraft.properties.clipLen = 1920;
    clipDraft.properties.gain = 1.0;
    clipDraft.defaultLanguage = QStringLiteral("unknown");
    const auto insertClip = runtime.project().insertClips(
        commandContext(runtime), {
                                     {.trackId = trackId, .clip = clipDraft}
    });
    ok &= expect(
        insertClip && insertClip.get().changed && insertClip.get().affectedObjects.size() == 1 &&
            insertClip.get().createdObjects ==
                QList<Automation::CreatedObjectRef>{
                    {QStringLiteral("clip-main"), insertClip.get().affectedObjects.first()}
    } &&
            track->clips().count() == 1,
        "singing clip insertion must bind client_ref through the project facade");
    const auto clipId = insertClip
                            ? Automation::ClipId(insertClip.get().affectedObjects.first().value)
                            : Automation::ClipId();
    auto *singingClip = dynamic_cast<SingingClip *>(model.findClipById(clipId.value()));
    ok &= expect(singingClip && singingClip->name() == clipDraft.properties.name,
                 "inserted singing clip must be addressable by its returned ID");

    const auto wrongClipType =
        runtime.project().confirmAudioClipPath(commandContext(runtime), clipId);
    ok &= expect(!wrongClipType && wrongClipType.getError().code ==
                                       Automation::AutomationErrorCode::WrongObjectType,
                 "typed object resolution must distinguish missing and wrong-type clips");

    Automation::NoteDraftDto noteDraft;
    noteDraft.clientRef = QStringLiteral("note-main");
    noteDraft.localStart = 0;
    noteDraft.length = 480;
    noteDraft.keyIndex = 60;
    noteDraft.lyric = QStringLiteral("la");
    const auto notePreview =
        runtime.notes().insertNotes(commandContext(runtime, true), clipId, {noteDraft});
    ok &=
        expect(notePreview && notePreview.get().validatedOnly &&
                   notePreview.get().createdObjects.isEmpty() && singingClip->notes().count() == 0,
               "note validate-only must not allocate IDs or attach notes");

    const auto beforeDuplicateNoteRefs = runtime.documentVersion();
    const auto duplicateNoteRefs =
        runtime.notes().insertNotes(commandContext(runtime), clipId, {noteDraft, noteDraft});
    ok &= expect(!duplicateNoteRefs &&
                     duplicateNoteRefs.getError().code ==
                         Automation::AutomationErrorCode::InvalidArgument &&
                     duplicateNoteRefs.getError().fieldPath == QStringLiteral("client_ref") &&
                     runtime.documentVersion() == beforeDuplicateNoteRefs &&
                     singingClip->notes().count() == 0,
                 "duplicate client_ref values must fail before allocation or partial commit");

    const auto insertNote =
        runtime.notes().insertNotes(commandContext(runtime), clipId, {noteDraft});
    ok &= expect(
        insertNote && insertNote.get().changed && insertNote.get().affectedObjects.size() == 1 &&
            insertNote.get().createdObjects ==
                QList<Automation::CreatedObjectRef>{
                    {QStringLiteral("note-main"), insertNote.get().affectedObjects.first()}
    } &&
            singingClip->notes().count() == 1,
        "note insertion must return the client_ref binding and inserted note ID");
    const auto noteId = insertNote
                            ? Automation::NoteId(insertNote.get().affectedObjects.first().value)
                            : Automation::NoteId();
    auto *note = singingClip->findNoteById(noteId.value());
    const auto projectAfterCreate =
        runtime.project().getProject(runtime.documentVersion().documentId);
    const auto notesAfterCreate =
        runtime.notes().getNotes(runtime.documentVersion().documentId, clipId);
    ok &=
        expect(projectAfterCreate && !projectAfterCreate.get().tracks.isEmpty() &&
                   projectAfterCreate.get().tracks.first().data.clientRef.isEmpty() &&
                   !projectAfterCreate.get().tracks.first().clips.isEmpty() &&
                   projectAfterCreate.get().tracks.first().clips.first().data.clientRef.isEmpty() &&
                   notesAfterCreate && !notesAfterCreate.get().isEmpty() &&
                   notesAfterCreate.get().first().data.clientRef.isEmpty(),
               "client_ref metadata must not persist in document snapshots");

    Automation::GuiDocumentCommandContext guiDocumentContext{
        .expected = runtime.documentVersion(),
        .windowId = runtime.windowId(),
        .source = Automation::InvocationSource::Test,
    };
    const auto selectTrack = runtime.facade().setSelectedTrack(guiDocumentContext, trackId);
    const auto selectClip = runtime.facade().setSelectedClips(guiDocumentContext, {clipId});
    const auto selectNote = runtime.facade().setSelectedNotes(guiDocumentContext, clipId, {noteId});
    Automation::EditorRevealDto revealTarget{
        .kind = Automation::EditorRevealKind::PianoRollNotes,
        .objectIds = {noteId.value()},
        .containerId = clipId.value(),
        .tickStart = 0.0,
        .tickEnd = 480.0,
        .valueStart = 60.0,
        .valueEnd = 60.0,
        .ticksAreLocal = true,
    };
    const auto revealNote = runtime.facade().reveal(guiDocumentContext, revealTarget);
    const auto selectedEditorState =
        runtime.facade().getEditorState(runtime.documentVersion().documentId, runtime.windowId());
    ok &= expect(selectTrack && selectClip && selectNote && revealNote && editorRevealApplied &&
                     editorStableState.selectedTrackIndex == 0 &&
                     editorStableState.selectedClipIds == QList<int>{clipId.value()} &&
                     editorStableState.activeClipId == clipId.value() &&
                     editorStableState.selectedNoteIds == QList<int>{noteId.value()} &&
                     selectedEditorState &&
                     selectedEditorState.get().selection.selectedTrackId == trackId &&
                     runtime.documentVersion() == guiDocumentContext.expected,
                 "selection and reveal must use explicit document and window routing without "
                 "revision changes");
    auto staleGuiDocumentContext = guiDocumentContext;
    staleGuiDocumentContext.expected.revision -= 1;
    const auto staleSelection =
        runtime.facade().setSelectedClips(staleGuiDocumentContext, {Automation::ClipId(999999)});
    ok &= expect(!staleSelection && staleSelection.getError().code ==
                                        Automation::AutomationErrorCode::RevisionConflict,
                 "GUI document commands must validate revision before object IDs");

    const auto beforeNoteNoOp = runtime.documentVersion();
    const auto noteNoOp =
        runtime.notes().moveNotes(commandContext(runtime), clipId, {noteId}, 0, 0);
    ok &= expect(noteNoOp && !noteNoOp.get().changed && runtime.documentVersion() == beforeNoteNoOp,
                 "zero-distance note movement must be a successful no-op");

    const auto moveNote =
        runtime.notes().moveNotes(commandContext(runtime), clipId, {noteId}, 120, 2);
    ok &= expect(moveNote && moveNote.get().changed && note && note->localStart() == 120 &&
                     note->keyIndex() == 62,
                 "note movement must update time and key in one revision");

    Automation::CurveDraftDto curveDraft;
    curveDraft.type = Automation::CurveDraftDto::Type::Draw;
    curveDraft.localStart = 120;
    curveDraft.step = 5;
    curveDraft.values = {6000, 6025, 6050};
    const auto replaceParameter = runtime.parameters().replaceParameter(
        commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited, {curveDraft});
    ok &= expect(replaceParameter && replaceParameter.get().changed,
                 "parameter replacement must commit through the parameter facade");
    const auto parameter = runtime.parameters().getParameter(
        runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
    ok &= expect(parameter && parameter.get().curves.size() == 1 &&
                     parameter.get().curves.first().values == curveDraft.values,
                 "parameter query must return a value snapshot of edited curves");

    const auto parameterVersion = runtime.documentVersion();
    const auto parameterNoOp = runtime.parameters().replaceParameter(
        commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited, {curveDraft});
    ok &= expect(parameterNoOp && !parameterNoOp.get().changed &&
                     runtime.documentVersion() == parameterVersion,
                 "replacing a parameter with identical curves must not add history or revision");

    auto copiedClipDraft = Automation::clipDraftDto(*singingClip);
    copiedClipDraft.properties.start = 2400;
    const auto insertCopiedClip = runtime.project().insertClips(
        commandContext(runtime), {
                                     {.trackId = trackId, .clip = copiedClipDraft}
    });
    const auto copiedClipId =
        insertCopiedClip ? Automation::ClipId(insertCopiedClip.get().affectedObjects.first().value)
                         : Automation::ClipId();
    auto *copiedClip = dynamic_cast<SingingClip *>(model.findClipById(copiedClipId.value()));
    const auto copiedParameter =
        copiedClip
            ? runtime.parameters().getParameter(runtime.documentVersion().documentId, copiedClipId,
                                                ParamInfo::Pitch, Param::Edited)
            : Automation::AutomationResult<Automation::ParameterSnapshotDto>(
                  Automation::AutomationError::notFound(
                      {Automation::ObjectKind::Clip, copiedClipId.value()},
                      QStringLiteral("Copied clip was not found")));
    ok &= expect(insertCopiedClip && copiedClip && copiedClip->notes().count() == 1 &&
                     copiedParameter && copiedParameter.get().curves.size() == 1 &&
                     copiedParameter.get().curves.first().values == curveDraft.values,
                 "clip value DTO copying must preserve notes and edited parameters");

    const auto undoCopiedClip = runtime.history().undo(commandContext(runtime));
    ok &= expect(undoCopiedClip && undoCopiedClip.get().changed &&
                     model.findClipById(copiedClipId.value()) == nullptr,
                 "a copied clip insertion must undo as one history entry");

    const auto selectOwnVoice =
        runtime.parameters().selectClipSingleSpeaker(commandContext(runtime), clipId, {}, {});
    ok &= expect(selectOwnVoice && selectOwnVoice.get().changed &&
                     !singingClip->usesTrackVoiceContext(),
                 "clip voice selection must switch from inherited to owned context");
    const auto useTrackVoice =
        runtime.parameters().useTrackVoiceContext(commandContext(runtime), clipId);
    ok &=
        expect(useTrackVoice && useTrackVoice.get().changed && singingClip->usesTrackVoiceContext(),
               "clip voice context must switch back to track inheritance through the facade");

    const auto setLanguage = runtime.project().setSingingClipDefaultLanguage(
        commandContext(runtime), clipId, QStringLiteral("en"));
    const auto languageVersion = runtime.documentVersion();
    const auto languageNoOp = runtime.project().setSingingClipDefaultLanguage(
        commandContext(runtime), clipId, QStringLiteral("en"));
    ok &= expect(setLanguage && setLanguage.get().changed &&
                     singingClip->defaultLanguage() == "en" && languageNoOp &&
                     !languageNoOp.get().changed && runtime.documentVersion() == languageVersion,
                 "non-history document state must still advance revision exactly once");

    Automation::BatchImportDraftDto batchImport;
    batchImport.timeline = model.timeline();
    Automation::BatchImportItemDraftDto batchItem;
    batchItem.newTrack.clientRef = QStringLiteral("batch-track");
    batchItem.newTrack.name = QStringLiteral("Imported Audio");
    batchItem.newTrack.defaultLanguage = QStringLiteral("unknown");
    Automation::ClipDraftDto audioDraft;
    audioDraft.clientRef = QStringLiteral("batch-clip");
    audioDraft.type = Automation::ClipDraftDto::Type::Audio;
    audioDraft.properties.name = QStringLiteral("audio.wav");
    audioDraft.properties.length = 480;
    audioDraft.properties.clipLen = 480;
    audioDraft.audioPath = QStringLiteral("D:/audio.wav");
    batchItem.clips.append(audioDraft);
    batchImport.items.append(batchItem);
    const auto tracksBeforeBatch = model.tracks().size();
    const auto batchPreview =
        runtime.project().commitBatchImport(commandContext(runtime, true), batchImport);
    const auto beforeBatch = runtime.documentVersion();
    const auto batchResult =
        runtime.project().commitBatchImport(commandContext(runtime), batchImport);
    const auto audioId = batchResult && batchResult.get().affectedObjects.size() == 2
                             ? Automation::ClipId(batchResult.get().affectedObjects.at(1).value)
                             : Automation::ClipId();
    auto *audioClip = dynamic_cast<AudioClip *>(model.findClipById(audioId.value()));
    ok &= expect(
        batchPreview && batchPreview.get().validatedOnly &&
            batchPreview.get().createdObjects.isEmpty() && batchResult &&
            batchResult.get().current.revision == beforeBatch.revision + 1 &&
            batchResult.get().createdObjects.size() == 2 &&
            batchResult.get().createdObjects.at(0).clientRef == QStringLiteral("batch-track") &&
            batchResult.get().createdObjects.at(0).object.kind == Automation::ObjectKind::Track &&
            batchResult.get().createdObjects.at(1).clientRef == QStringLiteral("batch-clip") &&
            batchResult.get().createdObjects.at(1).object ==
                Automation::ObjectRef{Automation::ObjectKind::Clip, audioId.value()} &&
            model.tracks().size() == tracksBeforeBatch + 1 && audioClip,
        "prepared batch import must bind client refs and commit once");

    if (audioClip) {
        const auto cacheRevision = runtime.documentVersion();
        AudioInfoModel audioInfo;
        audioInfo.sampleRate = 48000;
        audioInfo.channels = 1;
        audioInfo.frames = 48000;
        audioInfo.peakCache.append({-10, 10});
        const auto decodeCache = runtime.project().applyAudioDecodeCache(
            commandContext(runtime), audioId, audioClip->path(), audioInfo);
        const auto setStatus = runtime.project().setAudioClipPathStatus(
            commandContext(runtime), audioId, audioClip->path(), AudioClip::PathStatus::Missing);
        const auto setHash = runtime.project().setAudioClipHash(
            commandContext(runtime), audioId, audioClip->path(), QStringLiteral("abc123"));
        const auto resolvePath = runtime.project().applyResolvedAudioPath(
            commandContext(runtime), audioId, audioClip->path(), QStringLiteral("D:/resolved.wav"),
            AudioClip::PathStatus::Normal);
        ok &= expect(decodeCache && setStatus && setHash && resolvePath &&
                         runtime.documentVersion() == cacheRevision &&
                         audioClip->audioInfo().sampleRate == 48000 &&
                         audioClip->pathInfo().sha512 == QStringLiteral("abc123") &&
                         audioClip->path() == QStringLiteral("D:/resolved.wav"),
                     "derived audio writeback must validate snapshots without advancing revision");
    }

    const auto undoBatch = runtime.history().undo(commandContext(runtime));
    ok &= expect(undoBatch && undoBatch.get().changed &&
                     model.findClipById(audioId.value()) == nullptr &&
                     model.tracks().size() == tracksBeforeBatch,
                 "batch import must undo as one history entry");

    Automation::ClipDraftDto extractionAudioDraft;
    extractionAudioDraft.type = Automation::ClipDraftDto::Type::Audio;
    extractionAudioDraft.properties.name = QStringLiteral("extraction.wav");
    extractionAudioDraft.properties.start = 0;
    extractionAudioDraft.properties.length = 960;
    extractionAudioDraft.properties.clipLen = 960;
    extractionAudioDraft.audioPath = QStringLiteral("D:/extraction.wav");
    const auto insertExtractionAudio = runtime.project().insertClips(
        commandContext(runtime), {
                                     {.trackId = trackId, .clip = extractionAudioDraft}
    });
    const auto extractionAudioId =
        insertExtractionAudio
            ? Automation::ClipId(insertExtractionAudio.get().affectedObjects.first().value)
            : Automation::ClipId();
    ok &= expect(insertExtractionAudio && model.findClipById(extractionAudioId.value()),
                 "extraction tests require an addressable audio clip");

    const auto wrongPitchAudio =
        runtime.extractions().startPitch(commandContext(runtime, true), clipId, clipId);
    const auto wrongMidiAudio =
        runtime.extractions().startMidi(commandContext(runtime, true), clipId);
    ok &= expect(
        !wrongPitchAudio &&
            wrongPitchAudio.getError().code == Automation::AutomationErrorCode::WrongObjectType &&
            !wrongMidiAudio &&
            wrongMidiAudio.getError().code == Automation::AutomationErrorCode::WrongObjectType,
        "extraction commands must resolve typed clip IDs before preparing jobs");

    const auto tasksBeforePitchValidation = runtime.automationTasks().size();
    const auto pitchValidation =
        runtime.extractions().startPitch(commandContext(runtime, true), extractionAudioId, clipId);
    const auto validationPitchState = pitchExtractionStates.last();
    ok &= expect(pitchValidation && pitchValidation.get().validatedOnly &&
                     pitchValidation.get().taskId.isNull() &&
                     runtime.automationTasks().size() == tasksBeforePitchValidation &&
                     validationPitchState->startCount == 0 && scheduledExtractions.isEmpty(),
                 "pitch extraction validation must not allocate or start an automation task");

    auto pitchContext = commandContext(runtime);
    pitchContext.idempotencyKey = QStringLiteral("594dd2bc-f341-42bf-aeb0-6ed95965f776");
    const auto pitchBase = runtime.documentVersion();
    const auto pitchAccepted =
        runtime.extractions().startPitch(pitchContext, extractionAudioId, clipId);
    const auto pitchState = pitchExtractionStates.last();
    if (!scheduledExtractions.isEmpty())
        scheduledExtractions.takeFirst()();
    if (pitchState->completed) {
        pitchState->completed({
            .state = Automation::ExtractionBackendState::Succeeded,
            .segments = {{singingClip->start() + 40, {61.0, 61.5}}},
        });
    }
    const auto pitchReplay =
        runtime.extractions().startPitch(pitchContext, extractionAudioId, clipId);
    const auto pitchTask = pitchAccepted
                               ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                         pitchAccepted.get().taskId)
                               : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                     Automation::AutomationError{});
    const auto extractedPitch = runtime.parameters().getParameter(
        runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
    ok &= expect(pitchAccepted && pitchReplay && pitchReplay.get() == pitchAccepted.get() &&
                     pitchState->startCount == 1 && pitchTask &&
                     pitchTask.get().state == Automation::AutomationTaskState::Succeeded &&
                     pitchTask.get().progress.value == 25 && pitchTask.get().mutation &&
                     pitchTask.get().mutation->current.revision == pitchBase.revision + 1 &&
                     extractedPitch && extractedPitch.get().curves.size() == 1 &&
                     extractedPitch.get().curves.first().values == QList<int>({6100, 6150}),
                 "pitch extraction must replay one task and commit one parameter revision");

    const auto tracksBeforeMidiExtraction = model.tracks().size();
    const auto midiBase = runtime.documentVersion();
    const auto midiAccepted =
        runtime.extractions().startMidi(commandContext(runtime), extractionAudioId);
    const auto midiState = midiExtractionStates.last();
    if (!scheduledExtractions.isEmpty())
        scheduledExtractions.takeFirst()();
    if (midiState->completed) {
        midiState->completed({
            .state = Automation::ExtractionBackendState::Succeeded,
            .notes = {{64, 20, 240}, {67, 300, 360}},
        });
    }
    const auto midiTask = midiAccepted
                              ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                        midiAccepted.get().taskId)
                              : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                    Automation::AutomationError{});
    auto *midiTrack = model.tracks().isEmpty() ? nullptr : model.tracks().last();
    auto *midiClip = midiTrack && midiTrack->clips().count() > 0
                         ? dynamic_cast<SingingClip *>(midiTrack->clips().toList().first())
                         : nullptr;
    ok &= expect(midiAccepted && midiState->startCount == 1 && midiTask &&
                     midiTask.get().state == Automation::AutomationTaskState::Succeeded &&
                     midiTask.get().mutation &&
                     midiTask.get().mutation->current.revision == midiBase.revision + 1 &&
                     model.tracks().size() == tracksBeforeMidiExtraction + 1 && midiClip &&
                     midiClip->notes().count() == 2 &&
                     midiClip->notes().toList().first()->lyric() == QStringLiteral("啦"),
                 "MIDI extraction must commit one typed track with captured language defaults");

    const auto queuedPitch =
        runtime.extractions().startPitch(commandContext(runtime), extractionAudioId, clipId);
    const auto queuedPitchState = pitchExtractionStates.last();
    const auto queuedPitchCancel =
        runtime.tasks().cancelTask(commandContext(runtime), queuedPitch.get().taskId);
    if (!scheduledExtractions.isEmpty())
        scheduledExtractions.takeFirst()();
    const auto queuedPitchTask =
        runtime.tasks().getTask(runtime.documentVersion().documentId, queuedPitch.get().taskId);
    ok &= expect(queuedPitch && queuedPitchCancel && queuedPitchState->cancelCount == 1 &&
                     queuedPitchState->startCount == 0 && queuedPitchTask &&
                     queuedPitchTask.get().state == Automation::AutomationTaskState::Canceled,
                 "queued extraction cancellation must prevent backend execution");

    const auto runningMidi =
        runtime.extractions().startMidi(commandContext(runtime), extractionAudioId);
    const auto runningMidiState = midiExtractionStates.last();
    if (!scheduledExtractions.isEmpty())
        scheduledExtractions.takeFirst()();
    const auto runningMidiCancel =
        runtime.tasks().cancelTask(commandContext(runtime), runningMidi.get().taskId);
    if (runningMidiState->completed) {
        runningMidiState->completed({.state = Automation::ExtractionBackendState::Canceled});
    }
    const auto runningMidiTask =
        runtime.tasks().getTask(runtime.documentVersion().documentId, runningMidi.get().taskId);
    ok &=
        expect(runningMidi && runningMidiCancel && runningMidiState->startCount == 1 &&
                   runningMidiState->cancelCount == 1 && runningMidiTask &&
                   runningMidiTask.get().state == Automation::AutomationTaskState::Canceled,
               "running extraction cancellation must terminate the job and retain its task result");

    const auto failedMidi =
        runtime.extractions().startMidi(commandContext(runtime), extractionAudioId);
    const auto failedMidiState = midiExtractionStates.last();
    if (!scheduledExtractions.isEmpty())
        scheduledExtractions.takeFirst()();
    if (failedMidiState->completed) {
        failedMidiState->completed({
            .state = Automation::ExtractionBackendState::Failed,
            .errorCode = Automation::AutomationErrorCode::InferenceError,
            .errorMessage = QStringLiteral("simulated extraction failure"),
        });
    }
    const auto failedMidiTask =
        runtime.tasks().getTask(runtime.documentVersion().documentId, failedMidi.get().taskId);
    ok &= expect(failedMidi && failedMidiTask &&
                     failedMidiTask.get().state == Automation::AutomationTaskState::Failed &&
                     failedMidiTask.get().error &&
                     failedMidiTask.get().error->code ==
                         Automation::AutomationErrorCode::InferenceError,
                 "extraction backend failures must remain queryable with a stable error code");

    const auto stalePitch =
        runtime.extractions().startPitch(commandContext(runtime), extractionAudioId, clipId);
    const auto stalePitchState = pitchExtractionStates.last();
    if (!scheduledExtractions.isEmpty())
        scheduledExtractions.takeFirst()();
    const auto interveningEdit = runtime.timeline().setTempo(commandContext(runtime), 1920, 123.0);
    if (stalePitchState->completed) {
        stalePitchState->completed({
            .state = Automation::ExtractionBackendState::Succeeded,
            .segments = {{singingClip->start() + 40, {70.0}}},
        });
    }
    const auto stalePitchTask =
        runtime.tasks().getTask(runtime.documentVersion().documentId, stalePitch.get().taskId);
    ok &= expect(stalePitch && interveningEdit && stalePitchTask &&
                     stalePitchTask.get().state == Automation::AutomationTaskState::Failed &&
                     stalePitchTask.get().error &&
                     stalePitchTask.get().error->code ==
                         Automation::AutomationErrorCode::RevisionConflict,
                 "extraction completion must reject writeback after an intervening edit");

    auto saveContext = commandContext(runtime);
    saveContext.idempotencyKey = QStringLiteral("7e1f1564-5335-43b8-a464-50db58c1ef2c");
    const auto beforeSave = runtime.documentVersion();
    const auto save = runtime.documents().saveDocument(
        saveContext, QStringLiteral("D:/automation-contract-test.dspx"));
    const auto saveReplay = runtime.documents().saveDocument(
        saveContext, QStringLiteral("D:/automation-contract-test.dspx"));
    const auto savedDocument =
        runtime.documents().getDocument(runtime.documentVersion().documentId);
    ok &= expect(
        save && saveReplay && save.get() == saveReplay.get() && save.get().changed &&
            runtime.documentVersion() == beforeSave && saveCount == 1 && savedDocument &&
            savedDocument.get().path == QStringLiteral("D:/automation-contract-test.dspx") &&
            savedDocument.get().saved,
        "save must preserve revision, update identity, set savepoint, and replay idempotently");

    int cancelCount = 0;
    const auto task = runtime.automationTasks().createTask(
        QStringLiteral("extract.pitch.start"), runtime.documentVersion(),
        Automation::ObjectRef{Automation::ObjectKind::Clip, clipId.value()},
        [&cancelCount] { ++cancelCount; });
    ok &= expect(runtime.automationTasks().markRunning(task.taskId),
                 "queued automation task must enter running state once");

    const auto cancelPreview =
        runtime.tasks().cancelTask(commandContext(runtime, true), task.taskId);
    const auto runningTask =
        runtime.tasks().getTask(runtime.documentVersion().documentId, task.taskId);
    ok &= expect(
        cancelPreview && cancelPreview.get().validatedOnly &&
            cancelPreview.get().state == Automation::AutomationTaskState::CancelRequested &&
            runningTask && runningTask.get().state == Automation::AutomationTaskState::Running &&
            cancelCount == 0,
        "task cancel validate-only must predict without invoking cancellation");

    const auto canceledRequest = runtime.tasks().cancelTask(commandContext(runtime), task.taskId);
    const auto repeatedCancel = runtime.tasks().cancelTask(commandContext(runtime), task.taskId);
    ok &= expect(
        canceledRequest && repeatedCancel &&
            canceledRequest.get().state == Automation::AutomationTaskState::CancelRequested &&
            repeatedCancel.get().state == Automation::AutomationTaskState::CancelRequested &&
            cancelCount == 1,
        "task cancellation must be idempotent and invoke its callback once");
    ok &= expect(runtime.automationTasks().cancel(task.taskId),
                 "task worker must be able to acknowledge cancellation");
    const auto terminalCancel = runtime.tasks().cancelTask(commandContext(runtime), task.taskId);
    ok &= expect(terminalCancel &&
                     terminalCancel.get().state == Automation::AutomationTaskState::Canceled &&
                     cancelCount == 1,
                 "cancel-after-terminal must return the stable terminal result");

    const auto committingTask = runtime.automationTasks().createTask(
        QStringLiteral("extract.pitch.start"), runtime.documentVersion());
    runtime.automationTasks().markRunning(committingTask.taskId);
    const auto beganCommit = runtime.automationTasks().beginCommitting(committingTask.taskId);
    const auto lateCancel =
        runtime.tasks().cancelTask(commandContext(runtime), committingTask.taskId);
    ok &= expect(beganCommit && beganCommit.get() && !lateCancel &&
                     lateCancel.getError().code ==
                         Automation::AutomationErrorCode::OperationNotCancelable,
                 "automation task must reject cancellation after its commit point");

    auto replacedAudioConfig = audioExportConfig;
    replacedAudioConfig.fileName = QStringLiteral("replaced.wav");
    const auto replacedAudioAccepted =
        runtime.audioExports().start(commandContext(runtime), replacedAudioConfig, {});
    auto replacedAudioExecution = std::move(scheduledAudioExport);
    scheduledAudioExport = {};
    const auto replacedPitchAccepted =
        runtime.extractions().startPitch(commandContext(runtime), extractionAudioId, clipId);
    const auto replacedPitchState = pitchExtractionStates.last();
    std::function<void()> replacedPitchExecution;
    if (!scheduledExtractions.isEmpty())
        replacedPitchExecution = scheduledExtractions.takeFirst();

    AppModel replacementModel;
    replacementModel.newProject();
    const auto replacementData = replacementModel.takeProjectData();
    const LoopSettings replacementLoop(true, 480, 960);
    const auto replacementDraft = Automation::documentDraftDto(replacementData, replacementLoop);
    const auto oldTaskDocument = runtime.documentVersion().documentId;
    const auto tasksBeforeReplacement = runtime.automationTasks().size();
    auto replacementWithIdempotency = commandContext(runtime, true);
    replacementWithIdempotency.idempotencyKey =
        QStringLiteral("b82a540d-c0ec-4aac-b240-a691c25713e1");
    const auto rejectedReplacementIdempotency = runtime.documents().commitOpenedDocument(
        replacementWithIdempotency, replacementDraft, QStringLiteral("D:/replacement.dspx"),
        QStringLiteral("replacement.dspx"), true);
    const auto replacementPreview = runtime.documents().commitOpenedDocument(
        commandContext(runtime, true), replacementDraft, QStringLiteral("D:/replacement.dspx"),
        QStringLiteral("replacement.dspx"), true);
    ok &= expect(
        !rejectedReplacementIdempotency &&
            rejectedReplacementIdempotency.getError().code ==
                Automation::AutomationErrorCode::InvalidArgument &&
            rejectedReplacementIdempotency.getError().fieldPath ==
                QStringLiteral("idempotency_key") &&
            replacementPreview && replacementPreview.get().validatedOnly &&
            replacementPreview.get().current.documentId.isNull() &&
            runtime.documentVersion().documentId == oldTaskDocument &&
            runtime.automationTasks().size() == tasksBeforeReplacement,
        "document replace validate-only must not allocate an ID or alter the active session");

    const auto replacement = runtime.documents().commitOpenedDocument(
        commandContext(runtime), replacementDraft, QStringLiteral("D:/replacement.dspx"),
        QStringLiteral("replacement.dspx"), true);
    if (replacedAudioExecution)
        replacedAudioExecution();
    if (replacedPitchExecution)
        replacedPitchExecution();
    const auto replacedDocument =
        runtime.documents().getDocument(runtime.documentVersion().documentId);
    const auto staleDocument = runtime.documents().getDocument(oldTaskDocument);
    ok &= expect(
        replacedAudioAccepted && replacedPitchAccepted && replacement &&
            replacement.get().changed && replacement.get().previous.documentId == oldTaskDocument &&
            replacement.get().current == runtime.documentVersion() &&
            runtime.automationTasks().size() == 0 &&
            runtime.documentVersion().documentId != oldTaskDocument &&
            runtime.documentVersion().revision == 0 && audioExportState->executeCount == 2 &&
            replacedPitchState->cancelCount == 1 && replacedPitchState->startCount == 0 &&
            appliedLoopSettings == replacementLoop && replacedDocument &&
            replacedDocument.get().path == QStringLiteral("D:/replacement.dspx") &&
            !staleDocument &&
            staleDocument.getError().code == Automation::AutomationErrorCode::DocumentChanged,
        "document replacement must atomically rotate identity and invalidate old tasks");

    Automation::InferenceMutationRequest inferenceRequest;
    inferenceRequest.kind = Automation::InferenceMutationKind::ResetStage;
    inferenceRequest.clipId = Automation::ClipId(1);
    inferenceRequest.pieceId = Automation::PieceId(2);
    inferenceRequest.stage = Automation::InferenceStage::Pitch;
    const auto inferenceVersion = runtime.documentVersion();
    const auto inferencePreview =
        runtime.inference().applyMutation(commandContext(runtime, true), inferenceRequest);
    const auto inferenceApply =
        runtime.inference().applyMutation(commandContext(runtime), inferenceRequest);
    ok &= expect(inferencePreview && inferencePreview.get().mutation.validatedOnly &&
                     inferencePreview.get().mutation.changed && inferenceApply &&
                     inferenceApply.get().mutation.changed && inferenceApplyCount == 2 &&
                     inferencePreview.get().mutation.current.revision ==
                         inferenceVersion.revision + 1 &&
                     runtime.documentVersion().revision == inferenceVersion.revision + 1,
                 "persisted inference writeback must preview and advance revision once");

    history->reset();

    return ok ? 0 : 1;
}
