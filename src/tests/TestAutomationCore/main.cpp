#include "Automation/AutomationDispatcher.h"
#include "Automation/CoreRuntime.h"
#include "Automation/ProjectAutomationDtos.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCoreApplication>
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
            return Automation::AutomationError::documentChanged(documentId,
                                                                m_first.documentId());
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
    ok &= expect(!conflict &&
                     conflict.getError().code == Automation::AutomationErrorCode::IdempotencyConflict,
                 "same key with another request must fail with idempotency conflict");

    const auto oldDocumentId = first.documentId();
    first.replaceGeneration({}, QStringLiteral("Replacement"));
    ok &= expect(first.idempotencyStore().size() == 0,
                 "replacing the generation must clear idempotency records");
    const auto stale = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), oldDocumentId,
        [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &= expect(!stale && stale.getError().code == Automation::AutomationErrorCode::DocumentChanged,
                 "old document ID must fail after generation replacement");

    const auto invalidWindow = window.validateWindow(Automation::WindowId::create());
    ok &= expect(!invalidWindow &&
                     invalidWindow.getError().code ==
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
    EditorViewState editorViewState;
    Automation::EditorRuntimeServices editorServices;
    editorServices.captureView = [&editorViewState] {
        return std::optional<EditorViewState>(editorViewState);
    };
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
    editorServices.centerPianoRoll = [&editorViewState](const double tick,
                                                       const double keyIndex) {
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
    Automation::SettingsSnapshotDto applicationSettings;
    applicationSettings.general.uiLanguage = QStringLiteral("system");
    applicationSettings.general.defaultSingingLanguage = QStringLiteral("cmn");
    applicationSettings.general.defaultLyrics.insert(QStringLiteral("cmn"),
                                                      QStringLiteral("啦"));
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
    settingsServices.applyGeneral = [&applicationSettings,
                                     &settingsWriteCount](const auto &value) {
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
    settingsServices.applyWindow = [&applicationSettings,
                                    &settingsWriteCount](const auto &value) {
        applicationSettings.window = value;
        ++settingsWriteCount;
        return true;
    };
    settingsServices.applyAudio = [&applicationSettings,
                                   &settingsWriteCount](const auto &value) {
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
        return QList<Automation::PackageDto>{{
            .id = QStringLiteral("voice.package"),
            .version = QVersionNumber(1, 0),
            .path = QStringLiteral("packages/voice.package"),
        }};
    };
    packageServices.validatePackage = [](const QString &) {
        return Automation::AutomationResult<Automation::PackageValidationReportDto>(
            Automation::PackageValidationReportDto{});
    };
    int packageResolveApplyCount = 0;
    packageServices.resolveDocumentVoices = [&packageResolveApplyCount](AppModel *, const bool apply) {
        if (apply)
            ++packageResolveApplyCount;
        return 1;
    };
    int inferenceApplyCount = 0;
    Automation::InferenceRuntimeServices inferenceServices;
    inferenceServices.prepareMutation = [&inferenceApplyCount](
                                            AppModel *,
                                            const Automation::InferenceMutationRequest &request) {
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
    Automation::CoreRuntime runtime(&model, history, std::move(documentServices),
                                    std::move(playbackServices), std::move(editorServices),
                                    std::move(settingsServices), std::move(presetServices),
                                    std::move(packageServices), std::move(inferenceServices));
    const auto state = runtime.facade().getEditorState(runtime.windowId());
    const auto capabilities = runtime.facade().getEditorCapabilities();
    ok &= expect(state && state.get().document == runtime.documentVersion(),
                 "editor state must include the current document version");
    ok &= expect(capabilities && capabilities.get().maxConcurrentDocuments == 1 &&
                     capabilities.get().maxConcurrentWindows == 1,
                 "capabilities must declare the single document/window boundary");
    ok &= expect(capabilities && capabilities.get().operationIds.size() == 104 &&
                     capabilities.get().operationIds.contains(QStringLiteral("history.undo")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("tempos.set")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("tracks.insert")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("notes.insert")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("parameters.replace")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("imports.commit_batch")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("operations.cancel")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("playback.play")) &&
                     capabilities.get().operationIds.contains(
                         QStringLiteral("settings.update_audio")) &&
                     capabilities.get().operationIds.contains(
                         QStringLiteral("speaker_mix_presets.save")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("packages.validate")) &&
                     capabilities.get().operationIds.contains(
                         QStringLiteral("inference.apply_phoneme_names")) &&
                     capabilities.get().operationIds.contains(
                         QStringLiteral("editor.set_piano_roll_edit_mode")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("documents.save")),
                 "capabilities must be derived from every registered operation");

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
    const auto playbackPosition =
        runtime.playback().setPosition(commandContext(runtime), 960.0);
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

    Automation::GuiCommandContext guiContext{
        .windowId = runtime.windowId(),
        .source = Automation::InvocationSource::Test,
    };
    auto guiPreviewContext = guiContext;
    guiPreviewContext.validateOnly = true;
    const auto editorPreview = runtime.facade().centerPianoRoll(guiPreviewContext, 1440.0, 72.0);
    const auto editorCenter = runtime.facade().centerPianoRoll(guiContext, 1440.0, 72.0);
    const auto editorMode = runtime.facade().setPianoRollEditMode(
        guiContext, EditorViewGlobal::DrawNote);
    ok &= expect(editorPreview && editorPreview.get().validatedOnly && editorCenter && editorMode &&
                     editorViewState.pianoRoll.centerTick == 1440.0 &&
                     editorViewState.pianoRoll.centerKeyIndex == 72.0 &&
                     editorViewState.pianoRoll.editMode == EditorViewGlobal::DrawNote,
                 "GUI editor commands must validate and route through the single window context");
    guiContext.windowId = Automation::WindowId::create();
    const auto unknownEditorWindow =
        runtime.facade().setPanelVisibility(guiContext, true, false);
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
                     settingsPreview.get().changed && settingsUpdate && settingsUpdate.get().changed &&
                     settingsNoOp && !settingsNoOp.get().changed && settingsWriteCount == 1,
                 "application settings must support validation and no-op persistence");
    auto invalidAppearance = applicationSettings.appearance;
    invalidAppearance.animationTimeScale = 0.0;
    const auto rejectedAppearance = runtime.settings().updateAppearance({}, invalidAppearance);
    ok &= expect(!rejectedAppearance &&
                     rejectedAppearance.getError().code ==
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
    const auto presetDelete = presetSave
                                  ? runtime.presets().deleteSpeakerMixPreset({}, presetSave.get().id)
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
    const auto packageResolve =
        runtime.packages().resolveDocumentVoices(commandContext(runtime));
    ok &= expect(installedPackages && installedPackages.get().size() == 1 && packageValidation &&
                     !packageValidation.get().hasErrors && !invalidPackageValidation &&
                     invalidPackageValidation.getError().code ==
                         Automation::AutomationErrorCode::InvalidArgument && packageResolvePreview &&
                     packageResolvePreview.get().validatedOnly && packageResolve &&
                     packageResolve.get().changed && packageResolveApplyCount == 1 &&
                     runtime.documentVersion() == packageResolveVersion,
                 "package operations must expose typed reports and guarded cache resolution");

    Automation::CommandContext setTempoContext;
    setTempoContext.expected = runtime.documentVersion();
    const auto setTempo = runtime.timeline().setTempo(setTempoContext, 960, 150.0);
    ok &= expect(setTempo && setTempo.get().changed &&
                     setTempo.get().current.revision == 1 && hasTempoAt(model, 960, 150.0),
                 "timeline mutation must commit one action and one revision");

    Automation::CommandContext noOpContext;
    noOpContext.expected = runtime.documentVersion();
    const auto noOp = runtime.timeline().setTempo(noOpContext, 960, 150.0);
    ok &= expect(noOp && !noOp.get().changed && runtime.documentVersion().revision == 1,
                 "legal no-op must not record history or advance revision");

    Automation::CommandContext validateContext;
    validateContext.expected = runtime.documentVersion();
    validateContext.validateOnly = true;
    const auto preview = runtime.timeline().setTempo(validateContext, 960, 160.0);
    ok &= expect(preview && preview.get().validatedOnly && preview.get().changed &&
                     preview.get().current.revision == 2 &&
                     runtime.documentVersion().revision == 1 && hasTempoAt(model, 960, 150.0),
                 "validate-only must predict the result without changing model or revision");

    Automation::CommandContext staleContext;
    staleContext.expected = {runtime.documentVersion().documentId, 0};
    const auto staleMutation = runtime.timeline().setTempo(staleContext, 960, 160.0);
    ok &= expect(!staleMutation &&
                     staleMutation.getError().code ==
                         Automation::AutomationErrorCode::RevisionConflict,
                 "revision validation must precede domain mutation");

    const auto historyState = runtime.history().getState(runtime.documentVersion().documentId);
    ok &= expect(historyState && historyState.get().canUndo && !historyState.get().canRedo,
                 "history query must reflect the committed timeline action");

    Automation::CommandContext undoContext;
    undoContext.expected = runtime.documentVersion();
    const auto undo = runtime.history().undo(undoContext);
    ok &= expect(undo && undo.get().changed && undo.get().current.revision == 2 &&
                     !hasTempoAt(model, 960, 150.0),
                 "undo must use the same revision-owning commit path");

    Automation::CommandContext redoContext;
    redoContext.expected = runtime.documentVersion();
    const auto redo = runtime.history().redo(redoContext);
    ok &= expect(redo && redo.get().changed && redo.get().current.revision == 3 &&
                     hasTempoAt(model, 960, 150.0),
                 "redo must use the same revision-owning commit path");

    Automation::CommandContext emptyRedoContext;
    emptyRedoContext.expected = runtime.documentVersion();
    const auto emptyRedo = runtime.history().redo(emptyRedoContext);
    ok &= expect(emptyRedo && !emptyRedo.get().changed && runtime.documentVersion().revision == 3,
                 "empty redo must be a successful no-op");

    history->reset();

    Automation::TrackDraftDto trackDraft;
    trackDraft.name = QStringLiteral("Automation Track");
    trackDraft.gain = 1.0;
    trackDraft.defaultLanguage = QStringLiteral("unknown");

    const auto trackPreview = runtime.project().insertTrack(commandContext(runtime, true), 0,
                                                            trackDraft);
    ok &= expect(trackPreview && trackPreview.get().validatedOnly && trackPreview.get().changed &&
                     trackPreview.get().current.revision == runtime.documentVersion().revision + 1 &&
                     model.tracks().isEmpty(),
                 "track validate-only must not allocate or insert a track");

    const auto insertTrack = runtime.project().insertTrack(commandContext(runtime), 0, trackDraft);
    ok &= expect(insertTrack && insertTrack.get().changed &&
                     insertTrack.get().affectedObjects.size() == 1 && model.tracks().size() == 1,
                 "track insertion must commit once and return the new track ID");
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
    ok &= expect(!wrongDocument &&
                     wrongDocument.getError().code ==
                         Automation::AutomationErrorCode::DocumentChanged,
                 "document ID validation must precede revision and object validation");

    Automation::CommandContext staleObjectContext = commandContext(runtime);
    staleObjectContext.expected.revision -= 1;
    const auto staleObject =
        runtime.project().setTrackColor(staleObjectContext, Automation::TrackId(999999), 1);
    ok &= expect(!staleObject &&
                     staleObject.getError().code ==
                         Automation::AutomationErrorCode::RevisionConflict,
                 "revision validation must precede object validation");

    Automation::ClipDraftDto clipDraft;
    clipDraft.type = Automation::ClipDraftDto::Type::Singing;
    clipDraft.properties.name = QStringLiteral("Automation Clip");
    clipDraft.properties.start = 0;
    clipDraft.properties.length = 1920;
    clipDraft.properties.clipStart = 0;
    clipDraft.properties.clipLen = 1920;
    clipDraft.properties.gain = 1.0;
    clipDraft.defaultLanguage = QStringLiteral("unknown");
    const auto insertClip = runtime.project().insertClips(
        commandContext(runtime), {{.trackId = trackId, .clip = clipDraft}});
    ok &= expect(insertClip && insertClip.get().changed &&
                     insertClip.get().affectedObjects.size() == 1 && track->clips().count() == 1,
                 "singing clip insertion must commit through the project facade");
    const auto clipId = insertClip
                            ? Automation::ClipId(insertClip.get().affectedObjects.first().value)
                            : Automation::ClipId();
    auto *singingClip = dynamic_cast<SingingClip *>(model.findClipById(clipId.value()));
    ok &= expect(singingClip && singingClip->name() == clipDraft.properties.name,
                 "inserted singing clip must be addressable by its returned ID");

    const auto wrongClipType =
        runtime.project().confirmAudioClipPath(commandContext(runtime), clipId);
    ok &= expect(!wrongClipType &&
                     wrongClipType.getError().code ==
                         Automation::AutomationErrorCode::WrongObjectType,
                 "typed object resolution must distinguish missing and wrong-type clips");

    Automation::NoteDraftDto noteDraft;
    noteDraft.localStart = 0;
    noteDraft.length = 480;
    noteDraft.keyIndex = 60;
    noteDraft.lyric = QStringLiteral("la");
    const auto notePreview = runtime.notes().insertNotes(commandContext(runtime, true), clipId,
                                                        {noteDraft});
    ok &= expect(notePreview && notePreview.get().validatedOnly &&
                     singingClip->notes().count() == 0,
                 "note validate-only must not allocate or attach notes");

    const auto insertNote =
        runtime.notes().insertNotes(commandContext(runtime), clipId, {noteDraft});
    ok &= expect(insertNote && insertNote.get().changed &&
                     insertNote.get().affectedObjects.size() == 1 &&
                     singingClip->notes().count() == 1,
                 "note insertion must return the inserted note ID");
    const auto noteId = insertNote
                            ? Automation::NoteId(insertNote.get().affectedObjects.first().value)
                            : Automation::NoteId();
    auto *note = singingClip->findNoteById(noteId.value());

    const auto beforeNoteNoOp = runtime.documentVersion();
    const auto noteNoOp =
        runtime.notes().moveNotes(commandContext(runtime), clipId, {noteId}, 0, 0);
    ok &= expect(noteNoOp && !noteNoOp.get().changed &&
                     runtime.documentVersion() == beforeNoteNoOp,
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
        commandContext(runtime), {{.trackId = trackId, .clip = copiedClipDraft}});
    const auto copiedClipId =
        insertCopiedClip
            ? Automation::ClipId(insertCopiedClip.get().affectedObjects.first().value)
            : Automation::ClipId();
    auto *copiedClip = dynamic_cast<SingingClip *>(model.findClipById(copiedClipId.value()));
    const auto copiedParameter = copiedClip
                                     ? runtime.parameters().getParameter(
                                           runtime.documentVersion().documentId, copiedClipId,
                                           ParamInfo::Pitch, Param::Edited)
                                     : Automation::AutomationResult<
                                           Automation::ParameterSnapshotDto>(
                                           Automation::AutomationError::notFound(
                                               {Automation::ObjectKind::Clip,
                                                copiedClipId.value()},
                                               QStringLiteral("Copied clip was not found")));
    ok &= expect(insertCopiedClip && copiedClip && copiedClip->notes().count() == 1 &&
                     copiedParameter && copiedParameter.get().curves.size() == 1 &&
                     copiedParameter.get().curves.first().values == curveDraft.values,
                 "clip value DTO copying must preserve notes and edited parameters");

    const auto undoCopiedClip = runtime.history().undo(commandContext(runtime));
    ok &= expect(undoCopiedClip && undoCopiedClip.get().changed &&
                     model.findClipById(copiedClipId.value()) == nullptr,
                 "a copied clip insertion must undo as one history entry");

    const auto selectOwnVoice = runtime.parameters().selectClipSingleSpeaker(
        commandContext(runtime), clipId, {}, {});
    ok &= expect(selectOwnVoice && selectOwnVoice.get().changed &&
                     !singingClip->usesTrackVoiceContext(),
                 "clip voice selection must switch from inherited to owned context");
    const auto useTrackVoice =
        runtime.parameters().useTrackVoiceContext(commandContext(runtime), clipId);
    ok &= expect(useTrackVoice && useTrackVoice.get().changed &&
                     singingClip->usesTrackVoiceContext(),
                 "clip voice context must switch back to track inheritance through the facade");

    const auto setLanguage = runtime.project().setSingingClipDefaultLanguage(
        commandContext(runtime), clipId, QStringLiteral("en"));
    const auto languageVersion = runtime.documentVersion();
    const auto languageNoOp = runtime.project().setSingingClipDefaultLanguage(
        commandContext(runtime), clipId, QStringLiteral("en"));
    ok &= expect(setLanguage && setLanguage.get().changed && singingClip->defaultLanguage() == "en" &&
                     languageNoOp && !languageNoOp.get().changed &&
                     runtime.documentVersion() == languageVersion,
                 "non-history document state must still advance revision exactly once");

    Automation::BatchImportDraftDto batchImport;
    batchImport.timeline = model.timeline();
    Automation::BatchImportItemDraftDto batchItem;
    batchItem.newTrack.name = QStringLiteral("Imported Audio");
    batchItem.newTrack.defaultLanguage = QStringLiteral("unknown");
    Automation::ClipDraftDto audioDraft;
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
    const auto audioId =
        batchResult && batchResult.get().affectedObjects.size() == 2
            ? Automation::ClipId(batchResult.get().affectedObjects.at(1).value)
            : Automation::ClipId();
    auto *audioClip = dynamic_cast<AudioClip *>(model.findClipById(audioId.value()));
    ok &= expect(batchPreview && batchPreview.get().validatedOnly && batchResult &&
                     batchResult.get().current.revision == beforeBatch.revision + 1 &&
                     model.tracks().size() == tracksBeforeBatch + 1 && audioClip,
                 "prepared batch import must validate without allocation and commit once");

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
    ok &= expect(undoBatch && undoBatch.get().changed && model.findClipById(audioId.value()) == nullptr &&
                     model.tracks().size() == tracksBeforeBatch,
                 "batch import must undo as one history entry");

    auto saveContext = commandContext(runtime);
    saveContext.idempotencyKey = QStringLiteral("7e1f1564-5335-43b8-a464-50db58c1ef2c");
    const auto beforeSave = runtime.documentVersion();
    const auto save = runtime.documents().saveDocument(
        saveContext, QStringLiteral("D:/automation-contract-test.dspx"));
    const auto saveReplay = runtime.documents().saveDocument(
        saveContext, QStringLiteral("D:/automation-contract-test.dspx"));
    const auto savedDocument = runtime.documents().getDocument(runtime.documentVersion().documentId);
    ok &= expect(save && saveReplay && save.get() == saveReplay.get() && save.get().changed &&
                     runtime.documentVersion() == beforeSave && saveCount == 1 && savedDocument &&
                     savedDocument.get().path == QStringLiteral("D:/automation-contract-test.dspx") &&
                     savedDocument.get().saved,
                 "save must preserve revision, update identity, set savepoint, and replay idempotently");

    int cancelCount = 0;
    const auto task = runtime.automationTasks().createTask(
        QStringLiteral("parameters.extract_pitch"), runtime.documentVersion(),
        Automation::ObjectRef{Automation::ObjectKind::Clip, clipId.value()},
        [&cancelCount] { ++cancelCount; });
    ok &= expect(runtime.automationTasks().markRunning(task.taskId),
                 "queued automation task must enter running state once");

    const auto cancelPreview =
        runtime.tasks().cancelTask(commandContext(runtime, true), task.taskId);
    const auto runningTask = runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                     task.taskId);
    ok &= expect(cancelPreview && cancelPreview.get().validatedOnly &&
                     cancelPreview.get().state == Automation::AutomationTaskState::CancelRequested &&
                     runningTask && runningTask.get().state == Automation::AutomationTaskState::Running &&
                     cancelCount == 0,
                 "task cancel validate-only must predict without invoking cancellation");

    const auto canceledRequest = runtime.tasks().cancelTask(commandContext(runtime), task.taskId);
    const auto repeatedCancel = runtime.tasks().cancelTask(commandContext(runtime), task.taskId);
    ok &= expect(canceledRequest && repeatedCancel &&
                     canceledRequest.get().state ==
                         Automation::AutomationTaskState::CancelRequested &&
                     repeatedCancel.get().state ==
                         Automation::AutomationTaskState::CancelRequested &&
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
        QStringLiteral("parameters.extract_pitch"), runtime.documentVersion());
    runtime.automationTasks().markRunning(committingTask.taskId);
    const auto beganCommit = runtime.automationTasks().beginCommitting(committingTask.taskId);
    const auto lateCancel =
        runtime.tasks().cancelTask(commandContext(runtime), committingTask.taskId);
    ok &= expect(beganCommit && beganCommit.get() && !lateCancel &&
                     lateCancel.getError().code ==
                         Automation::AutomationErrorCode::OperationNotCancelable,
                 "automation task must reject cancellation after its commit point");

    AppModel replacementModel;
    replacementModel.newProject();
    const auto replacementData = replacementModel.takeProjectData();
    const LoopSettings replacementLoop(true, 480, 960);
    const auto replacementDraft =
        Automation::documentDraftDto(replacementData, replacementLoop);
    const auto oldTaskDocument = runtime.documentVersion().documentId;
    const auto replacementPreview = runtime.documents().commitOpenedDocument(
        commandContext(runtime, true), replacementDraft, QStringLiteral("D:/replacement.dspx"),
        QStringLiteral("replacement.dspx"), true);
    ok &= expect(replacementPreview && replacementPreview.get().validatedOnly &&
                     replacementPreview.get().current.documentId.isNull() &&
                     runtime.documentVersion().documentId == oldTaskDocument &&
                     runtime.automationTasks().size() == 2,
                 "document replace validate-only must not allocate an ID or alter the active session");

    const auto replacement = runtime.documents().commitOpenedDocument(
        commandContext(runtime), replacementDraft, QStringLiteral("D:/replacement.dspx"),
        QStringLiteral("replacement.dspx"), true);
    const auto replacedDocument = runtime.documents().getDocument(runtime.documentVersion().documentId);
    const auto staleDocument = runtime.documents().getDocument(oldTaskDocument);
    ok &= expect(replacement && replacement.get().changed &&
                     replacement.get().previous.documentId == oldTaskDocument &&
                     replacement.get().current == runtime.documentVersion() &&
                     runtime.automationTasks().size() == 0 &&
                     runtime.documentVersion().documentId != oldTaskDocument &&
                     runtime.documentVersion().revision == 0 &&
                     appliedLoopSettings == replacementLoop && replacedDocument &&
                     replacedDocument.get().path == QStringLiteral("D:/replacement.dspx") &&
                     !staleDocument && staleDocument.getError().code ==
                                           Automation::AutomationErrorCode::DocumentChanged,
                 "document replacement must atomically rotate identity and invalidate old tasks");

    Automation::InferenceMutationRequest inferenceRequest;
    inferenceRequest.kind = Automation::InferenceMutationKind::ResetStage;
    inferenceRequest.clipId = Automation::ClipId(1);
    inferenceRequest.pieceId = Automation::PieceId(2);
    inferenceRequest.stage = Automation::InferenceStage::Pitch;
    const auto inferenceVersion = runtime.documentVersion();
    const auto inferencePreview = runtime.inference().applyMutation(
        commandContext(runtime, true), inferenceRequest);
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
