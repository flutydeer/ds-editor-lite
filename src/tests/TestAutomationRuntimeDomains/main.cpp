#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "TestRuntime.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>

namespace {
    class TestLog final {
    public:
        void scenario(const QString &id) {
            ++m_scenarios;
            m_currentScenario = id;
        }

        bool expect(const bool condition, const QString &message) {
            ++m_assertions;
            if (condition)
                return true;
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_currentScenario << "]: " << message << Qt::endl;
            return false;
        }

        template <typename T>
        bool expectError(const Automation::AutomationResult<T> &result,
                         const Automation::AutomationErrorCode code,
                         const Automation::OperationId &operationId, const QString &message) {
            return expect(!result && result.getError().code == code &&
                              result.getError().operationId == operationId,
                          message);
        }

        int finish() const {
            QTextStream(stdout) << "Automation runtime domain coverage: " << m_scenarios
                                << " scenarios, " << m_assertions << " assertions, " << m_failures
                                << " failures" << Qt::endl;
            return m_failures == 0 ? 0 : 1;
        }

    private:
        QString m_currentScenario;
        int m_scenarios = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime,
                                              const bool validateOnly = false) {
        return {
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::GuiCommandContext guiContext(const Automation::CoreRuntime &runtime,
                                             const bool validateOnly = false) {
        return {
            .windowId = runtime.windowId(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::GuiDocumentCommandContext guiDocumentContext(const Automation::CoreRuntime &runtime,
                                                             const bool validateOnly = false) {
        return {
            .expected = runtime.documentVersion(),
            .windowId = runtime.windowId(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::ApplicationCommandContext applicationContext(const bool validateOnly = false) {
        return {
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::SettingsSnapshotDto validSettings() {
        Automation::SettingsSnapshotDto settings;
        settings.general.uiLanguage = QStringLiteral("system");
        settings.general.defaultSingingLanguage = QStringLiteral("cmn");
        settings.general.defaultLyrics.insert(QStringLiteral("cmn"), QStringLiteral("啦"));
        settings.appearance.themeId = QStringLiteral("system");
        settings.inference.executionProvider = QStringLiteral("CPU");
        settings.inference.cacheDirectory = QStringLiteral("cache");
        for (int index = 0; index < 4; ++index) {
            settings.audio.pseudoSingerSynthesizers.append({
                .generator = index,
                .amplitude = -12.0,
                .attackMilliseconds = 10,
                .decayMilliseconds = 1000,
                .decayRatio = 0.5,
                .releaseMilliseconds = 50,
            });
        }
        return settings;
    }

    class RuntimeHarness final {
    public:
        RuntimeHarness() : history(HistoryManager::instance()), settings(validSettings()) {
            history->reset();
            packages.append({
                .id = QStringLiteral("voice.package"),
                .version = QVersionNumber(2, 1),
                .vendor = QStringLiteral("Vendor"),
                .description = QStringLiteral("Description"),
                .license = QStringLiteral("License"),
                .readme = QStringLiteral("Readme"),
                .url = QStringLiteral("https://example.invalid/package"),
                .path = QStringLiteral("packages/voice.package"),
                .singers = {{
                    .singerId = QStringLiteral("singer"),
                    .packageId = QStringLiteral("voice.package"),
                    .packageVersion = QVersionNumber(2, 1),
                    .name = QStringLiteral("Singer"),
                }},
            });
            runtime = std::make_unique<Automation::CoreRuntime>(
                &model, history, Automation::DocumentRuntimeServices{}, playbackServices(),
                editorServices(), settingsServices(), presetServices(), packageServices(),
                Automation::InferenceRuntimeServices{}, Automation::FileRuntimeServices{},
                Automation::AudioExportRuntimeServices{}, Automation::ExtractionRuntimeServices{},
                applicationServices());
        }

        ~RuntimeHarness() {
            runtime.reset();
            history->reset();
        }

        Automation::CoreRuntime &core() {
            return *runtime;
        }

        void resetHistory() {
            history->reset();
        }

        Automation::PlaybackHostSnapshot playback;
        bool playbackCanStart = true;
        bool playbackPlaySucceeds = true;
        int playCalls = 0;
        int pauseCalls = 0;
        int stopCalls = 0;
        int positionCalls = 0;
        int lastPositionCalls = 0;
        int loopCalls = 0;

        EditorViewState editorView;
        Automation::EditorStableState editorStable;
        bool editorViewAvailable = true;
        bool editorApplySucceeds = true;
        bool editorRevealSucceeds = true;
        int editorApplyCalls = 0;
        int editorStableApplyCalls = 0;
        int revealCalls = 0;

        Automation::SettingsSnapshotDto settings;
        bool settingsApplySucceeds = true;
        int settingsWriteAttempts = 0;
        int settingsWrites = 0;

        QList<Automation::SpeakerMixPresetDto> presets;
        bool presetApplySucceeds = true;
        int presetWriteAttempts = 0;
        int presetWrites = 0;

        QList<Automation::PackageDto> packages;
        bool packageValidationSucceeds = true;
        int packageValidationCalls = 0;
        QString lastValidatedPackagePath;
        int packageResolveCount = 1;
        int packageResolvePreviewCalls = 0;
        int packageResolveApplyCalls = 0;

        Automation::ApplicationInfoDto applicationInfo{
            .name = QStringLiteral("DS Editor Lite"),
            .version = QStringLiteral("test-version"),
            .platform = QStringLiteral("test-platform"),
        };
        bool terminationSucceeds = true;
        int terminationCalls = 0;
        Automation::ApplicationTerminationMode lastTerminationMode =
            Automation::ApplicationTerminationMode::Exit;

    private:
        Automation::PlaybackRuntimeServices playbackServices() {
            Automation::PlaybackRuntimeServices services;
            services.snapshot = [this] { return playback; };
            services.canStart = [this] { return playbackCanStart; };
            services.play = [this] {
                ++playCalls;
                if (!playbackPlaySucceeds)
                    return false;
                playback.state = Automation::PlaybackState::Playing;
                return true;
            };
            services.pause = [this] {
                ++pauseCalls;
                playback.state = Automation::PlaybackState::Paused;
            };
            services.stop = [this] {
                ++stopCalls;
                playback.state = Automation::PlaybackState::Stopped;
            };
            services.setPosition = [this](const double tick) {
                ++positionCalls;
                playback.position = tick;
            };
            services.setLastPosition = [this](const double tick) {
                ++lastPositionCalls;
                playback.lastPosition = tick;
            };
            services.setLoop = [this](const LoopSettings &loop) {
                ++loopCalls;
                playback.loop = loop;
            };
            return services;
        }

        Automation::EditorRuntimeServices editorServices() {
            Automation::EditorRuntimeServices services;
            services.captureView = [this]() -> std::optional<EditorViewState> {
                if (!editorViewAvailable)
                    return std::nullopt;
                return editorView;
            };
            services.captureStableState = [this] { return editorStable; };
            services.restoreView = [this](const EditorViewState &state) {
                ++editorApplyCalls;
                if (!editorApplySucceeds)
                    return false;
                editorView = state;
                return true;
            };
            services.centerTrackPanel = [this](const double tick, const double index) {
                ++editorApplyCalls;
                if (!editorApplySucceeds)
                    return false;
                editorView.trackPanel.centerTick = tick;
                editorView.trackPanel.centerTrackIndex = index;
                return true;
            };
            services.setTrackPanelScale = [this](const double horizontal, const double vertical) {
                ++editorApplyCalls;
                if (!editorApplySucceeds)
                    return false;
                editorView.trackPanel.horizontalScale = horizontal;
                editorView.trackPanel.verticalScale = vertical;
                return true;
            };
            services.setPanelVisibility = [this](const bool track, const bool bottom) {
                ++editorApplyCalls;
                if (!editorApplySucceeds)
                    return false;
                editorView.layout.trackPanelVisible = track;
                editorView.layout.bottomPanelVisible = bottom;
                return true;
            };
            services.showBottomPanelPage = [this](const QString &pageId) {
                ++editorApplyCalls;
                if (!editorApplySucceeds)
                    return false;
                editorView.layout.bottomPanelPageId = pageId;
                return true;
            };
            services.centerPianoRoll = [this](const double tick, const double key) {
                ++editorApplyCalls;
                if (!editorApplySucceeds)
                    return false;
                editorView.pianoRoll.centerTick = tick;
                editorView.pianoRoll.centerKeyIndex = key;
                return true;
            };
            services.setPianoRollScale = [this](const double horizontal, const double vertical) {
                ++editorApplyCalls;
                if (!editorApplySucceeds)
                    return false;
                editorView.pianoRoll.horizontalScale = horizontal;
                editorView.pianoRoll.verticalScale = vertical;
                return true;
            };
            services.setPianoRollEditMode = [this](const auto mode) {
                ++editorApplyCalls;
                if (!editorApplySucceeds)
                    return false;
                editorView.pianoRoll.editMode = mode;
                return true;
            };
            services.setActiveClip = [this](const int clipId) {
                ++editorStableApplyCalls;
                if (editorStable.activeClipId != clipId)
                    editorStable.selectedNoteIds.clear();
                editorStable.activeClipId = clipId;
            };
            services.setSelectedTrackIndex = [this](const int index) {
                ++editorStableApplyCalls;
                editorStable.selectedTrackIndex = index;
            };
            services.setSelectedClips = [this](const QList<int> &ids) {
                ++editorStableApplyCalls;
                editorStable.selectedClipIds = ids;
            };
            services.setSelectedNotes = [this](const int clipId, const QList<int> &ids) {
                ++editorStableApplyCalls;
                editorStable.activeClipId = clipId;
                editorStable.selectedNoteIds = ids;
            };
            services.setPianoRollQuantize = [this](const int quantize, const bool enabled) {
                ++editorStableApplyCalls;
                editorStable.pianoRollQuantize = quantize;
                editorStable.pianoRollQuantizeEnabled = enabled;
            };
            services.setAutoPageTurn = [this](const auto target, const bool enabled) {
                ++editorStableApplyCalls;
                if (target == Automation::EditorAutoPageTarget::TrackPanel)
                    editorStable.trackAutoPageTurnEnabled = enabled;
                else
                    editorStable.pianoRollAutoPageTurnEnabled = enabled;
            };
            services.revealFocus = [this](const HistoryFocus &, const bool) {
                ++revealCalls;
                return editorRevealSucceeds;
            };
            return services;
        }

        template <typename T, typename Member>
        std::function<bool(const T &)> settingsApply(Member member) {
            return [this, member](const T &value) {
                ++settingsWriteAttempts;
                if (!settingsApplySucceeds)
                    return false;
                settings.*member = value;
                ++settingsWrites;
                return true;
            };
        }

        Automation::SettingsRuntimeServices settingsServices() {
            Automation::SettingsRuntimeServices services;
            services.snapshot = [this] { return settings; };
            services.applyGeneral = settingsApply<Automation::GeneralSettingsDto>(
                &Automation::SettingsSnapshotDto::general);
            services.applyAppearance = settingsApply<Automation::AppearanceSettingsDto>(
                &Automation::SettingsSnapshotDto::appearance);
            services.applyInference = settingsApply<Automation::InferenceSettingsDto>(
                &Automation::SettingsSnapshotDto::inference);
            services.applyDeveloper = settingsApply<Automation::DeveloperSettingsDto>(
                &Automation::SettingsSnapshotDto::developer);
            services.applyG2pLanguage = settingsApply<Automation::G2pLanguageSettingsDto>(
                &Automation::SettingsSnapshotDto::g2pLanguage);
            services.applyFillLyric = settingsApply<Automation::FillLyricSettingsDto>(
                &Automation::SettingsSnapshotDto::fillLyric);
            services.applyWindow = settingsApply<Automation::WindowSettingsDto>(
                &Automation::SettingsSnapshotDto::window);
            services.applyAudio = settingsApply<Automation::AudioSettingsDto>(
                &Automation::SettingsSnapshotDto::audio);
            return services;
        }

        Automation::PresetRuntimeServices presetServices() {
            Automation::PresetRuntimeServices services;
            services.speakerMixPresets = [this] { return presets; };
            services.applySpeakerMixPresets = [this](const auto &value) {
                ++presetWriteAttempts;
                if (!presetApplySucceeds)
                    return false;
                presets = value;
                ++presetWrites;
                return true;
            };
            return services;
        }

        Automation::PackageRuntimeServices packageServices() {
            Automation::PackageRuntimeServices services;
            services.installedPackages = [this] { return packages; };
            services.validatePackage = [this](const QString &path) {
                ++packageValidationCalls;
                lastValidatedPackagePath = path;
                if (!packageValidationSucceeds) {
                    Automation::AutomationError error;
                    error.code = Automation::AutomationErrorCode::IoError;
                    error.message = QStringLiteral("simulated package validation failure");
                    return Automation::AutomationResult<Automation::PackageValidationReportDto>(
                        std::move(error));
                }
                Automation::PackageValidationReportDto report;
                report.items.append({
                    .severity = Automation::PackageValidationSeverity::Warning,
                    .path = path,
                    .message = QStringLiteral("warning"),
                });
                return Automation::AutomationResult<Automation::PackageValidationReportDto>(report);
            };
            services.resolveDocumentVoices = [this](AppModel *, const bool apply) {
                if (apply)
                    ++packageResolveApplyCalls;
                else
                    ++packageResolvePreviewCalls;
                return packageResolveCount;
            };
            return services;
        }

        Automation::ApplicationRuntimeServices applicationServices() {
            Automation::ApplicationRuntimeServices services;
            services.info = [this] { return applicationInfo; };
            services.requestTermination = [this](const auto mode) {
                ++terminationCalls;
                lastTerminationMode = mode;
                return terminationSucceeds;
            };
            return services;
        }

        AppModel model;
        HistoryManager *history;
        std::unique_ptr<Automation::CoreRuntime> runtime;
    };

    struct EditorObjects {
        Automation::TrackId trackId;
        Automation::ClipId clipId;
        Automation::NoteId noteId;
    };

    std::optional<EditorObjects> createEditorObjects(RuntimeHarness &harness, TestLog &log) {
        Automation::TrackDraftDto track;
        track.clientRef = QStringLiteral("runtime-track");
        track.name = QStringLiteral("Runtime Track");
        track.gain = 1.0;
        track.defaultLanguage = QStringLiteral("unknown");

        Automation::ClipDraftDto clip;
        clip.clientRef = QStringLiteral("runtime-clip");
        clip.type = Automation::ClipDraftDto::Type::Singing;
        clip.properties.name = QStringLiteral("Runtime Clip");
        clip.properties.start = 0;
        clip.properties.length = 1920;
        clip.properties.clipStart = 0;
        clip.properties.clipLen = 1920;
        clip.properties.gain = 1.0;
        clip.defaultLanguage = QStringLiteral("unknown");
        clip.notes.append({
            .clientRef = QStringLiteral("runtime-note"),
            .localStart = 0,
            .length = 480,
            .keyIndex = 60,
            .lyric = QStringLiteral("la"),
            .language = QStringLiteral("unknown"),
        });
        track.clips.append(clip);

        const auto inserted =
            harness.core().project().insertTrack(commandContext(harness.core()), 0, track);
        if (!log.expect(bool(inserted), QStringLiteral("editor object fixture must be created")))
            return std::nullopt;

        EditorObjects objects;
        for (const auto &created : inserted.get().createdObjects) {
            switch (created.object.kind) {
                case Automation::ObjectKind::Track:
                    objects.trackId = Automation::TrackId(created.object.value);
                    break;
                case Automation::ObjectKind::Clip:
                    objects.clipId = Automation::ClipId(created.object.value);
                    break;
                case Automation::ObjectKind::Note:
                    objects.noteId = Automation::NoteId(created.object.value);
                    break;
                default:
                    break;
            }
        }
        if (!log.expect(objects.trackId.isValid() && objects.clipId.isValid() &&
                            objects.noteId.isValid(),
                        QStringLiteral("editor fixture must expose all strong IDs"))) {
            return std::nullopt;
        }
        harness.resetHistory();
        return objects;
    }

    void testApplicationLifecycle(TestLog &log) {
        RuntimeHarness harness;
        auto &runtime = harness.core();

        log.scenario(QStringLiteral("APP-Q-INFO-SNAPSHOT"));
        const auto info = runtime.application().getInfo();
        log.expect(info && info.get() == harness.applicationInfo,
                   QStringLiteral("application info must be returned as an owned snapshot"));

        log.scenario(QStringLiteral("APP-C-EXIT-VALIDATE"));
        const auto exitPreview = runtime.application().requestTermination(
            guiContext(runtime, true), Automation::ApplicationTerminationMode::Exit);
        log.expect(exitPreview && exitPreview.get().changed && exitPreview.get().validatedOnly &&
                       harness.terminationCalls == 0,
                   QStringLiteral("termination preview must not call the host"));

        log.scenario(QStringLiteral("APP-C-EXIT-COMMIT"));
        const auto exit = runtime.application().requestTermination(
            guiContext(runtime), Automation::ApplicationTerminationMode::Exit);
        log.expect(exit && exit.get().changed && !exit.get().validatedOnly &&
                       harness.terminationCalls == 1 &&
                       harness.lastTerminationMode == Automation::ApplicationTerminationMode::Exit,
                   QStringLiteral("exit must be mediated exactly once by the host"));

        log.scenario(QStringLiteral("APP-C-RESTART-COMMIT"));
        const auto restart = runtime.application().requestTermination(
            guiContext(runtime), Automation::ApplicationTerminationMode::Restart);
        log.expect(restart && harness.terminationCalls == 2 &&
                       harness.lastTerminationMode ==
                           Automation::ApplicationTerminationMode::Restart,
                   QStringLiteral("restart must preserve its distinct termination mode"));

        log.scenario(QStringLiteral("APP-C-HOST-REJECT"));
        harness.terminationSucceeds = false;
        const auto rejected = runtime.application().requestTermination(
            guiContext(runtime), Automation::ApplicationTerminationMode::Exit);
        log.expectError(rejected, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::application::request_exit,
                        QStringLiteral("host rejection must be a stable capability error"));

        log.scenario(QStringLiteral("APP-C-WINDOW-ISOLATION"));
        auto unknownWindow = guiContext(runtime);
        unknownWindow.windowId = Automation::WindowId::create();
        const auto unknown = runtime.application().requestTermination(
            unknownWindow, Automation::ApplicationTerminationMode::Restart);
        log.expectError(unknown, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::application::request_restart,
                        QStringLiteral("unknown window must be rejected before the host callback"));
        log.expect(harness.terminationCalls == 3,
                   QStringLiteral("unknown window must not invoke termination"));

        log.scenario(QStringLiteral("APP-C-INVALID-MODE-PRIORITY"));
        const auto invalidMode = runtime.application().requestTermination(
            unknownWindow, static_cast<Automation::ApplicationTerminationMode>(99));
        log.expect(!invalidMode &&
                       invalidMode.getError().code ==
                           Automation::AutomationErrorCode::InvalidArgument &&
                       invalidMode.getError().operationId.isEmpty(),
                   QStringLiteral("invalid mode is rejected before operation and window routing"));
    }

    void testPlaybackHostState(TestLog &log) {
        RuntimeHarness harness;
        auto &runtime = harness.core();
        const auto initialVersion = runtime.documentVersion();

        log.scenario(QStringLiteral("PLAY-Q-SNAPSHOT"));
        harness.playback.position = 120.0;
        harness.playback.lastPosition = 80.0;
        const auto snapshot = runtime.playback().getPlayback(initialVersion.documentId);
        log.expect(snapshot && snapshot.get().document == initialVersion &&
                       snapshot.get().position == 120.0 && snapshot.get().lastPosition == 80.0 &&
                       snapshot.get().state == Automation::PlaybackState::Stopped,
                   QStringLiteral("playback query must preserve host values and document version"));

        log.scenario(QStringLiteral("PLAY-Q-DOCUMENT-ISOLATION"));
        const auto wrongDocument = runtime.playback().getPlayback(Automation::DocumentId::create());
        log.expectError(wrongDocument, Automation::AutomationErrorCode::DocumentChanged,
                        Automation::OperationIds::playback::get,
                        QStringLiteral("playback query must reject another document"));

        log.scenario(QStringLiteral("PLAY-C-STATE-VALIDATE-NOOP"));
        const auto playPreview = runtime.playback().play(commandContext(runtime, true));
        log.expect(
            playPreview && playPreview.get().changed && playPreview.get().validatedOnly &&
                harness.playCalls == 0 && runtime.documentVersion() == initialVersion,
            QStringLiteral("play preview must predict without host or revision side effects"));
        const auto play = runtime.playback().play(commandContext(runtime));
        const auto playNoOp = runtime.playback().play(commandContext(runtime));
        log.expect(play && play.get().changed && playNoOp && !playNoOp.get().changed &&
                       harness.playCalls == 1 && runtime.documentVersion() == initialVersion,
                   QStringLiteral("play must call once and repeated play must be a no-op"));

        log.scenario(QStringLiteral("PLAY-C-PAUSE-STOP-NOOP"));
        const auto pause = runtime.playback().pause(commandContext(runtime));
        const auto pauseNoOp = runtime.playback().pause(commandContext(runtime));
        const auto stop = runtime.playback().stop(commandContext(runtime));
        const auto stopNoOp = runtime.playback().stop(commandContext(runtime));
        log.expect(pause && pause.get().changed && pauseNoOp && !pauseNoOp.get().changed && stop &&
                       stop.get().changed && stopNoOp && !stopNoOp.get().changed &&
                       harness.pauseCalls == 1 && harness.stopCalls == 1 &&
                       runtime.documentVersion() == initialVersion,
                   QStringLiteral("pause and stop must each suppress repeated host calls"));

        log.scenario(QStringLiteral("PLAY-C-BUSY"));
        harness.playbackCanStart = false;
        const auto busy = runtime.playback().play(commandContext(runtime));
        log.expectError(busy, Automation::AutomationErrorCode::Busy,
                        Automation::OperationIds::playback::play,
                        QStringLiteral("an active editor gesture must block playback start"));
        log.expect(harness.playCalls == 1,
                   QStringLiteral("busy playback must not call the device"));

        log.scenario(QStringLiteral("PLAY-C-DEVICE-FAILURE"));
        harness.playbackCanStart = true;
        harness.playbackPlaySucceeds = false;
        const auto failedStart = runtime.playback().play(commandContext(runtime));
        log.expectError(
            failedStart, Automation::AutomationErrorCode::HostCapabilityUnavailable,
            Automation::OperationIds::playback::play,
            QStringLiteral("device start failure must be reported without state change"));
        log.expect(harness.playback.state == Automation::PlaybackState::Stopped &&
                       runtime.documentVersion() == initialVersion,
                   QStringLiteral("failed playback start must keep state and revision"));

        log.scenario(QStringLiteral("PLAY-C-POSITION"));
        const auto positionPreview =
            runtime.playback().setPosition(commandContext(runtime, true), 960.0);
        const auto position = runtime.playback().setPosition(commandContext(runtime), 960.0);
        const auto positionNoOp = runtime.playback().setPosition(commandContext(runtime), 960.0);
        log.expect(positionPreview && positionPreview.get().validatedOnly &&
                       positionPreview.get().changed && position && position.get().changed &&
                       positionNoOp && !positionNoOp.get().changed &&
                       harness.playback.position == 960.0 && harness.positionCalls == 1 &&
                       runtime.documentVersion() == initialVersion,
                   QStringLiteral("position must support preview, commit and no-op"));

        log.scenario(QStringLiteral("PLAY-C-POSITION-BOUNDARIES"));
        const auto negativePosition = runtime.playback().setPosition(commandContext(runtime), -1.0);
        const auto nanPosition = runtime.playback().setPosition(
            commandContext(runtime), std::numeric_limits<double>::quiet_NaN());
        const auto infinitePosition = runtime.playback().setPosition(
            commandContext(runtime), std::numeric_limits<double>::infinity());
        log.expectError(negativePosition, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::playback::set_position,
                        QStringLiteral("negative playback position must be rejected"));
        log.expectError(nanPosition, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::playback::set_position,
                        QStringLiteral("NaN playback position must be rejected"));
        log.expectError(infinitePosition, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::playback::set_position,
                        QStringLiteral("infinite playback position must be rejected"));

        log.scenario(QStringLiteral("PLAY-C-LAST-POSITION"));
        const auto lastPreview =
            runtime.playback().setLastPosition(commandContext(runtime, true), 480.0);
        const auto last = runtime.playback().setLastPosition(commandContext(runtime), 480.0);
        const auto lastNoOp = runtime.playback().setLastPosition(commandContext(runtime), 480.0);
        const auto invalidLast = runtime.playback().setLastPosition(commandContext(runtime), -0.01);
        log.expect(lastPreview && lastPreview.get().validatedOnly && last && last.get().changed &&
                       lastNoOp && !lastNoOp.get().changed && harness.lastPositionCalls == 1 &&
                       harness.playback.lastPosition == 480.0,
                   QStringLiteral("last position must support preview, commit and no-op"));
        log.expectError(invalidLast, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::playback::set_last_position,
                        QStringLiteral("negative last position must be rejected"));

        log.scenario(QStringLiteral("PLAY-C-LOOP-REVISION"));
        const auto loopBase = runtime.documentVersion();
        const LoopSettings range(false, 480, 960);
        const auto loopPreview = runtime.playback().setLoop(commandContext(runtime, true), range);
        const auto loopSet = runtime.playback().setLoop(commandContext(runtime), range);
        const auto loopNoOp = runtime.playback().setLoop(commandContext(runtime), range);
        log.expect(loopPreview && loopPreview.get().validatedOnly && loopPreview.get().changed &&
                       loopPreview.get().current.revision == loopBase.revision + 1 && loopSet &&
                       loopSet.get().changed && loopNoOp && !loopNoOp.get().changed &&
                       runtime.documentVersion().revision == loopBase.revision + 1 &&
                       harness.loopCalls == 1,
                   QStringLiteral("loop range must record one revision and suppress no-op"));

        log.scenario(QStringLiteral("PLAY-C-LOOP-ENABLE-CLEAR"));
        const auto enable = runtime.playback().setLoopEnabled(commandContext(runtime), true);
        const auto enableNoOp = runtime.playback().setLoopEnabled(commandContext(runtime), true);
        const auto clear = runtime.playback().clearLoop(commandContext(runtime));
        const auto clearNoOp = runtime.playback().clearLoop(commandContext(runtime));
        log.expect(enable && enable.get().changed && enableNoOp && !enableNoOp.get().changed &&
                       clear && clear.get().changed && clearNoOp && !clearNoOp.get().changed &&
                       harness.playback.loop == LoopSettings() && harness.loopCalls == 3 &&
                       runtime.documentVersion().revision == loopBase.revision + 3,
                   QStringLiteral("enable and clear must each commit at most once"));

        log.scenario(QStringLiteral("PLAY-C-LOOP-VALIDATION"));
        const auto enableEmpty = runtime.playback().setLoopEnabled(commandContext(runtime), true);
        const auto zeroEnabled =
            runtime.playback().setLoop(commandContext(runtime), LoopSettings(true, 0, 0));
        const auto negativeRange =
            runtime.playback().setLoop(commandContext(runtime), LoopSettings(false, -1, 20));
        log.expectError(enableEmpty, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::playback::set_loop_enabled,
                        QStringLiteral("empty loop cannot be enabled"));
        log.expectError(zeroEnabled, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::playback::set_loop,
                        QStringLiteral("enabled zero-length loop must be rejected"));
        log.expectError(negativeRange, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::playback::set_loop,
                        QStringLiteral("negative loop range must be rejected"));

        log.scenario(QStringLiteral("PLAY-C-ERROR-PRIORITY"));
        auto stale = commandContext(runtime);
        ++stale.expected.revision;
        const auto staleInvalid = runtime.playback().setLoop(stale, LoopSettings(true, 0, 0));
        auto replaced = stale;
        replaced.expected.documentId = Automation::DocumentId::create();
        const auto replacedInvalid = runtime.playback().setLoop(replaced, LoopSettings(true, 0, 0));
        log.expectError(staleInvalid, Automation::AutomationErrorCode::RevisionConflict,
                        Automation::OperationIds::playback::set_loop,
                        QStringLiteral("revision must be checked before loop validation"));
        log.expectError(replacedInvalid, Automation::AutomationErrorCode::DocumentChanged,
                        Automation::OperationIds::playback::set_loop,
                        QStringLiteral("document must be checked before revision and loop"));

        log.scenario(QStringLiteral("PLAY-C-IDEMPOTENCY-UNSUPPORTED"));
        auto keyed = commandContext(runtime);
        keyed.idempotencyKey = QStringLiteral("playback-state-key");
        const auto unsupportedKey = runtime.playback().pause(keyed);
        log.expectError(
            unsupportedKey, Automation::AutomationErrorCode::InvalidArgument,
            Automation::OperationIds::playback::pause,
            QStringLiteral("ephemeral playback state must reject document idempotency"));
    }

    using GuiResult = Automation::AutomationResult<Automation::GuiMutationResult>;

    struct ViewCommandCase {
        QString scenarioId;
        Automation::OperationId operationId;
        std::function<GuiResult(RuntimeHarness &, const Automation::GuiCommandContext &)> invoke;
        std::function<GuiResult(RuntimeHarness &, const Automation::GuiCommandContext &)> invalid;
    };

    QList<ViewCommandCase> viewCommandCases() {
        EditorViewState restored;
        restored.trackPanel.centerTick = 240.0;
        restored.trackPanel.centerTrackIndex = 1.0;
        restored.trackPanel.horizontalScale = 2.0;
        restored.trackPanel.verticalScale = 1.5;
        restored.layout.bottomPanelPageId = QStringLiteral("Lyrics");
        restored.pianoRoll.centerTick = 480.0;
        restored.pianoRoll.centerKeyIndex = 72.0;
        restored.pianoRoll.horizontalScale = 1.25;
        restored.pianoRoll.verticalScale = 1.75;
        restored.pianoRoll.editMode = EditorViewGlobal::DrawNote;

        return {
            {
             .scenarioId = QStringLiteral("EDITOR-C-RESTORE-VIEW"),
             .operationId = Automation::OperationIds::editor::restore_view,
             .invoke =
                    [restored](RuntimeHarness &harness,                                                              const auto &context) {
                        return harness.core().facade().restoreView(context, restored);
                    },                                                               .invalid =
                    [](RuntimeHarness &harness,                   const auto &context) {
                        EditorViewState invalid;
                        invalid.layout.trackPanelVisible = false;
                        invalid.layout.bottomPanelVisible = false;
                        return harness.core().facade().restoreView(context, invalid);
                    }, },
            {
             .scenarioId = QStringLiteral("EDITOR-C-CENTER-TRACK"),
             .operationId = Automation::OperationIds::editor::center_track_panel,
             .invoke =
                    [](RuntimeHarness &harness,                                                  const auto &context) {
                        return harness.core().facade().centerTrackPanel(context, 240.0, 2.0);
                    },                                                           .invalid =
                    [](RuntimeHarness &harness,                                                                                                const auto &context) {
                        return harness.core().facade().centerTrackPanel(context, -1.0, 0.0);
                    }, },
            {
             .scenarioId = QStringLiteral("EDITOR-C-SCALE-TRACK"),
             .operationId = Automation::OperationIds::editor::set_track_panel_scale,
             .invoke =
                    [](RuntimeHarness &harness,                               const auto &context) {
                        return harness.core().facade().setTrackPanelScale(context, 2.0, 1.5);
                    },                                                     .invalid =
                    [](RuntimeHarness &harness,                                                                             const auto &context) {
                        return harness.core().facade().setTrackPanelScale(context, 0.0, 1.0);
                    }, },
            {
             .scenarioId = QStringLiteral("EDITOR-C-PANEL-VISIBILITY"),
             .operationId = Automation::OperationIds::editor::set_panel_visibility,
             .invoke =
                    [](RuntimeHarness &harness, const auto &context) {
                        return harness.core().facade().setPanelVisibility(context, true, false);
                    }, .invalid =
                    [](RuntimeHarness &harness, const auto &context) {
                        return harness.core().facade().setPanelVisibility(context, false, false);
                    }, },
            {
             .scenarioId = QStringLiteral("EDITOR-C-BOTTOM-PAGE"),
             .operationId = Automation::OperationIds::editor::show_bottom_panel_page,
             .invoke =
                    [](RuntimeHarness &harness,                                                                      const auto &context) {
                        return harness.core().facade().showBottomPanelPage(
                            context, QStringLiteral("Lyrics"));
                    },                                                                       .invalid =
                    [](RuntimeHarness &harness,                           const auto &context) {
                        return harness.core().facade().showBottomPanelPage(context,
                                                                           QStringLiteral(" "));
                    }, },
            {
             .scenarioId = QStringLiteral("EDITOR-C-CENTER-PIANO"),
             .operationId = Automation::OperationIds::editor::center_piano_roll,
             .invoke =
                    [](RuntimeHarness &harness,                                                   const auto &context) {
                        return harness.core().facade().centerPianoRoll(context, 480.0, 72.0);
                    },                                                       .invalid =
                    [](RuntimeHarness &harness,                                                                                                const auto &context) {
                        return harness.core().facade().centerPianoRoll(context, 0.0, 128.0);
                    }, },
            {
             .scenarioId = QStringLiteral("EDITOR-C-SCALE-PIANO"),
             .operationId = Automation::OperationIds::editor::set_piano_roll_scale,
             .invoke =
                    [](RuntimeHarness &harness,                               const auto &context) {
                        return harness.core().facade().setPianoRollScale(context, 1.25, 1.75);
                    },                                                     .invalid =
                    [](RuntimeHarness &harness,                                                                            const auto &context) {
                        return harness.core().facade().setPianoRollScale(
                            context, std::numeric_limits<double>::quiet_NaN(), 1.0);
                    }, },
            {
             .scenarioId = QStringLiteral("EDITOR-C-EDIT-MODE"),
             .operationId = Automation::OperationIds::editor::set_piano_roll_edit_mode,
             .invoke =
                    [](RuntimeHarness &harness, const auto &context) {
                        return harness.core().facade().setPianoRollEditMode(
                            context, EditorViewGlobal::DrawNote);
                    }, .invalid =
                    [](RuntimeHarness &harness, const auto &context) {
                        return harness.core().facade().setPianoRollEditMode(
                            context, static_cast<EditorViewGlobal::PianoRollEditMode>(99));
                    }, },
        };
    }

    void testEditorViewCommands(TestLog &log) {
        for (const auto &testCase : viewCommandCases()) {
            RuntimeHarness harness;
            auto &runtime = harness.core();
            log.scenario(testCase.scenarioId);

            const auto preview = testCase.invoke(harness, guiContext(runtime, true));
            const auto committed = testCase.invoke(harness, guiContext(runtime));
            const auto noOp = testCase.invoke(harness, guiContext(runtime));
            log.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                           committed && committed.get().changed && noOp && !noOp.get().changed &&
                           harness.editorApplyCalls == 1,
                       QStringLiteral("view command must preview, commit once, and detect no-op"));

            auto unknownWindow = guiContext(runtime);
            unknownWindow.windowId = Automation::WindowId::create();
            const auto unknown = testCase.invoke(harness, unknownWindow);
            log.expectError(unknown, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                            testCase.operationId,
                            QStringLiteral("view command must reject an unknown window"));
            log.expect(harness.editorApplyCalls == 1,
                       QStringLiteral("unknown window must not reach the view host"));

            const auto invalid = testCase.invalid(harness, guiContext(runtime));
            log.expect(!invalid && invalid.getError().code ==
                                       Automation::AutomationErrorCode::InvalidArgument,
                       QStringLiteral("view command must reject its invalid input"));

            harness.editorView = EditorViewState{};
            harness.editorApplySucceeds = false;
            const auto rejected = testCase.invoke(harness, guiContext(runtime));
            log.expectError(rejected, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                            testCase.operationId,
                            QStringLiteral("view host rejection must remain a stable error"));
            log.expect(harness.editorView == EditorViewState{},
                       QStringLiteral("view host rejection must not partially mutate state"));
        }

        RuntimeHarness harness;
        auto &runtime = harness.core();
        log.scenario(QStringLiteral("EDITOR-C-VIEW-HOST-UNAVAILABLE"));
        harness.editorViewAvailable = false;
        const auto unavailable = runtime.facade().centerPianoRoll(guiContext(runtime), 120.0, 60.0);
        log.expectError(unavailable, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::editor::center_piano_roll,
                        QStringLiteral("missing captured view must reject view mutation"));
    }

    void testEditorStateAndSelection(TestLog &log) {
        RuntimeHarness harness;
        auto &runtime = harness.core();

        log.scenario(QStringLiteral("EDITOR-Q-CAPABILITIES"));
        const auto capabilities = runtime.facade().getEditorCapabilities();
        log.expect(capabilities && capabilities.get().maxConcurrentDocuments == 1 &&
                       capabilities.get().maxConcurrentWindows == 1 &&
                       capabilities.get().operationIds == runtime.catalog().operationIds(),
                   QStringLiteral("capabilities must expose the single-session host and catalog"));

        log.scenario(QStringLiteral("EDITOR-SETUP-OBJECTS"));
        const auto objects = createEditorObjects(harness, log);
        if (!objects)
            return;
        const auto version = runtime.documentVersion();

        log.scenario(QStringLiteral("EDITOR-C-QUANTIZE"));
        const auto quantizePreview =
            runtime.facade().setPianoRollQuantize(guiContext(runtime, true), 24, false);
        const auto quantize = runtime.facade().setPianoRollQuantize(guiContext(runtime), 24, false);
        const auto quantizeNoOp =
            runtime.facade().setPianoRollQuantize(guiContext(runtime), 24, false);
        const auto invalidQuantize =
            runtime.facade().setPianoRollQuantize(guiContext(runtime), 7, true);
        log.expect(quantizePreview && quantizePreview.get().validatedOnly && quantize &&
                       quantize.get().changed && quantizeNoOp && !quantizeNoOp.get().changed &&
                       harness.editorStableApplyCalls == 1 && runtime.documentVersion() == version,
                   QStringLiteral("quantize must preview, commit, no-op and preserve revision"));
        log.expect(!invalidQuantize && invalidQuantize.getError().code ==
                                           Automation::AutomationErrorCode::InvalidArgument,
                   QStringLiteral("quantize must divide whole-note ticks"));

        log.scenario(QStringLiteral("EDITOR-C-AUTO-PAGE"));
        const auto autoPagePreview = runtime.facade().setAutoPageTurn(
            guiContext(runtime, true), Automation::EditorAutoPageTarget::TrackPanel, false);
        const auto autoPage = runtime.facade().setAutoPageTurn(
            guiContext(runtime), Automation::EditorAutoPageTarget::TrackPanel, false);
        const auto autoPageNoOp = runtime.facade().setAutoPageTurn(
            guiContext(runtime), Automation::EditorAutoPageTarget::TrackPanel, false);
        const auto invalidTarget = runtime.facade().setAutoPageTurn(
            guiContext(runtime), static_cast<Automation::EditorAutoPageTarget>(99), false);
        log.expect(autoPagePreview && autoPagePreview.get().validatedOnly && autoPage &&
                       autoPage.get().changed && autoPageNoOp && !autoPageNoOp.get().changed &&
                       harness.editorStableApplyCalls == 2,
                   QStringLiteral("auto-page must preview, commit once and detect no-op"));
        log.expect(!invalidTarget && invalidTarget.getError().code ==
                                         Automation::AutomationErrorCode::InvalidArgument,
                   QStringLiteral("unknown auto-page target must be rejected"));

        log.scenario(QStringLiteral("EDITOR-C-SELECTION-ROUNDTRIP"));
        auto context = guiDocumentContext(runtime);
        const auto selectTrack = runtime.facade().setSelectedTrack(context, objects->trackId);
        const auto selectClip = runtime.facade().setActiveClip(context, objects->clipId);
        const auto selectClips =
            runtime.facade().setSelectedClips(context, {objects->clipId, objects->clipId});
        const auto selectNotes = runtime.facade().setSelectedNotes(
            context, objects->clipId, {objects->noteId, objects->noteId});
        const auto selectNotesNoOp = runtime.facade().setSelectedNotes(
            context, objects->clipId, {objects->noteId, objects->noteId});
        log.expect(
            selectTrack && selectClip && selectClips && selectNotes && selectNotesNoOp &&
                !selectNotesNoOp.get().changed && harness.editorStable.selectedTrackIndex == 0 &&
                harness.editorStable.activeClipId == objects->clipId.value() &&
                harness.editorStable.selectedClipIds == QList<int>{objects->clipId.value()} &&
                harness.editorStable.selectedNoteIds == QList<int>{objects->noteId.value()} &&
                runtime.documentVersion() == version,
            QStringLiteral("selection must normalize IDs and not mutate the document"));

        log.scenario(QStringLiteral("EDITOR-Q-STATE-SNAPSHOT"));
        harness.editorView.pianoRoll.centerTick = 640.0;
        runtime.setDocumentBusy(version.documentId, true);
        const auto state = runtime.facade().getEditorState(version.documentId, runtime.windowId());
        log.expect(state && state.get().document == version &&
                       state.get().windowId == runtime.windowId() && state.get().documentBusy &&
                       state.get().view && state.get().view->pianoRoll.centerTick == 640.0 &&
                       state.get().selection.selectedTrackId == objects->trackId &&
                       state.get().selection.activeClipId == objects->clipId &&
                       state.get().selection.selectedClipIds ==
                           QList<Automation::ClipId>{objects->clipId} &&
                       state.get().selection.selectedNoteIds ==
                           QList<Automation::NoteId>{objects->noteId} &&
                       state.get().pianoRollQuantize == 24 && !state.get().trackAutoPageTurnEnabled,
                   QStringLiteral("editor state must combine session, view and stable selection"));
        runtime.setDocumentBusy(version.documentId, false);

        log.scenario(QStringLiteral("EDITOR-Q-OPTIONAL-VIEW"));
        harness.editorViewAvailable = false;
        const auto noViewState =
            runtime.facade().getEditorState(version.documentId, runtime.windowId());
        log.expect(
            noViewState && !noViewState.get().view,
            QStringLiteral("state query must remain available when no view snapshot exists"));
        harness.editorViewAvailable = true;

        log.scenario(QStringLiteral("EDITOR-Q-ID-ISOLATION"));
        const auto wrongDocument = runtime.facade().getEditorState(Automation::DocumentId::create(),
                                                                   Automation::WindowId::create());
        const auto wrongWindow =
            runtime.facade().getEditorState(version.documentId, Automation::WindowId::create());
        log.expectError(wrongDocument, Automation::AutomationErrorCode::DocumentChanged,
                        Automation::OperationIds::editor::get_state,
                        QStringLiteral("document must be resolved before window for state query"));
        log.expectError(wrongWindow, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::editor::get_state,
                        QStringLiteral("state query must reject unknown window"));

        log.scenario(QStringLiteral("EDITOR-C-SELECTION-ERROR-PRIORITY"));
        auto wrongDocumentContext = context;
        wrongDocumentContext.expected.documentId = Automation::DocumentId::create();
        wrongDocumentContext.expected.revision += 100;
        wrongDocumentContext.windowId = Automation::WindowId::create();
        const auto wrongDocumentSelection =
            runtime.facade().setActiveClip(wrongDocumentContext, Automation::ClipId(999999));
        auto staleContext = context;
        ++staleContext.expected.revision;
        staleContext.windowId = Automation::WindowId::create();
        const auto staleSelection =
            runtime.facade().setActiveClip(staleContext, Automation::ClipId(999999));
        auto wrongWindowContext = context;
        wrongWindowContext.windowId = Automation::WindowId::create();
        const auto wrongWindowSelection =
            runtime.facade().setActiveClip(wrongWindowContext, Automation::ClipId(999999));
        const auto missingClip =
            runtime.facade().setActiveClip(context, Automation::ClipId(999999));
        log.expectError(wrongDocumentSelection, Automation::AutomationErrorCode::DocumentChanged,
                        Automation::OperationIds::editor::set_active_clip,
                        QStringLiteral("document must win over revision, window and object"));
        log.expectError(staleSelection, Automation::AutomationErrorCode::RevisionConflict,
                        Automation::OperationIds::editor::set_active_clip,
                        QStringLiteral("revision must win over window and object"));
        log.expectError(wrongWindowSelection,
                        Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::editor::set_active_clip,
                        QStringLiteral("window must win over object resolution"));
        log.expectError(missingClip, Automation::AutomationErrorCode::NotFound,
                        Automation::OperationIds::editor::set_active_clip,
                        QStringLiteral("valid routing must reach object resolution"));

        log.scenario(QStringLiteral("EDITOR-C-REVEAL"));
        Automation::EditorRevealDto target{
            .kind = Automation::EditorRevealKind::PianoRollNotes,
            .objectIds = {objects->noteId.value()},
            .containerId = objects->clipId.value(),
            .tickStart = 0.0,
            .tickEnd = 480.0,
            .valueStart = 60.0,
            .valueEnd = 60.0,
            .ticksAreLocal = true,
        };
        const auto revealPreview =
            runtime.facade().reveal(guiDocumentContext(runtime, true), target, false);
        const auto reveal = runtime.facade().reveal(context, target, true);
        log.expect(revealPreview && revealPreview.get().validatedOnly && reveal &&
                       reveal.get().changed && harness.revealCalls == 1 &&
                       runtime.documentVersion() == version,
                   QStringLiteral("reveal must validate without host action and then apply once"));

        log.scenario(QStringLiteral("EDITOR-C-REVEAL-FALLBACK"));
        target.objectIds = {999999};
        target.allowRangeFallback = true;
        const auto fallback = runtime.facade().reveal(context, target);
        log.expect(fallback && harness.revealCalls == 2,
                   QStringLiteral("range fallback must tolerate a deleted note ID"));

        log.scenario(QStringLiteral("EDITOR-C-REVEAL-FAILURES"));
        target.allowRangeFallback = false;
        const auto missingNote = runtime.facade().reveal(context, target);
        target.objectIds = {objects->noteId.value()};
        target.tickStart = 10.0;
        target.tickEnd = 5.0;
        const auto invalidRange = runtime.facade().reveal(context, target);
        target.tickStart = 0.0;
        target.tickEnd = 480.0;
        harness.editorRevealSucceeds = false;
        const auto hostRejected = runtime.facade().reveal(context, target);
        log.expectError(missingNote, Automation::AutomationErrorCode::NotFound,
                        Automation::OperationIds::editor::reveal,
                        QStringLiteral("reveal must reject a missing note without fallback"));
        log.expectError(invalidRange, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::editor::reveal,
                        QStringLiteral("reveal must reject an inverted range"));
        log.expectError(hostRejected, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::editor::reveal,
                        QStringLiteral("reveal host rejection must be stable"));
    }

    template <typename T, typename Getter, typename Update, typename Mutate>
    void exerciseSettingsCategory(TestLog &log, RuntimeHarness &harness, const QString &scenarioId,
                                  const Automation::OperationId &operationId, Getter getter,
                                  Update update, Mutate mutate,
                                  const std::function<void(T &)> &invalidate,
                                  const std::function<void(T &)> &mutateFailure) {
        log.scenario(scenarioId);
        const auto original = getter(harness.settings);
        const auto attemptsBefore = harness.settingsWriteAttempts;
        const auto writesBefore = harness.settingsWrites;
        const auto noOp = update(applicationContext(), original);

        auto target = original;
        mutate(target);
        const auto preview = update(applicationContext(true), target);
        const auto committed = update(applicationContext(), target);
        const auto duplicate = update(applicationContext(), target);
        log.expect(noOp && !noOp.get().changed && preview && preview.get().validatedOnly &&
                       preview.get().changed && committed && committed.get().changed && duplicate &&
                       !duplicate.get().changed && getter(harness.settings) == target &&
                       harness.settingsWriteAttempts == attemptsBefore + 1 &&
                       harness.settingsWrites == writesBefore + 1,
                   QStringLiteral("settings category must support no-op, preview and one commit"));

        if (invalidate) {
            auto invalidValue = target;
            invalidate(invalidValue);
            const auto invalid = update(applicationContext(), invalidValue);
            log.expectError(invalid, Automation::AutomationErrorCode::InvalidArgument, operationId,
                            QStringLiteral("invalid settings value must be rejected"));
            log.expect(getter(harness.settings) == target,
                       QStringLiteral("invalid settings value must not mutate storage"));
        }

        auto failingValue = target;
        mutateFailure(failingValue);
        harness.settingsApplySucceeds = false;
        const auto failed = update(applicationContext(), failingValue);
        harness.settingsApplySucceeds = true;
        log.expectError(failed, Automation::AutomationErrorCode::IoError, operationId,
                        QStringLiteral("settings persistence failure must be reported"));
        log.expect(getter(harness.settings) == target && harness.settingsWrites == writesBefore + 1,
                   QStringLiteral("failed settings persistence must not alter the snapshot"));
    }

    void testSettingsDomains(TestLog &log) {
        RuntimeHarness harness;
        auto &runtime = harness.core();

        log.scenario(QStringLiteral("SETTINGS-Q-SNAPSHOT"));
        const auto snapshot = runtime.settings().getSettings();
        log.expect(snapshot && snapshot.get() == harness.settings,
                   QStringLiteral("settings query must return all eight domains exactly"));

        exerciseSettingsCategory<Automation::GeneralSettingsDto>(
            log, harness, QStringLiteral("SETTINGS-C-GENERAL"),
            Automation::OperationIds::settings::update_general,
            [](const auto &all) { return all.general; },
            [&runtime](const auto &context, const auto &value) {
                return runtime.settings().updateGeneral(context, value);
            },
            [](auto &value) { value.gameDirectory = QStringLiteral("game-data"); },
            [](auto &value) { value.uiLanguage = QStringLiteral("unsupported"); },
            [](auto &value) { value.pitchModelPath = QStringLiteral("model.bin"); });

        exerciseSettingsCategory<Automation::AppearanceSettingsDto>(
            log, harness, QStringLiteral("SETTINGS-C-APPEARANCE"),
            Automation::OperationIds::settings::update_appearance,
            [](const auto &all) { return all.appearance; },
            [&runtime](const auto &context, const auto &value) {
                return runtime.settings().updateAppearance(context, value);
            },
            [](auto &value) {
                value.animationTimeScale = 1.5;
                value.themeId = QStringLiteral("dark");
            },
            [](auto &value) { value.animationTimeScale = 0.0; },
            [](auto &value) { value.uiFontFamily = QStringLiteral("Test Font"); });

        exerciseSettingsCategory<Automation::InferenceSettingsDto>(
            log, harness, QStringLiteral("SETTINGS-C-INFERENCE"),
            Automation::OperationIds::settings::update_inference,
            [](const auto &all) { return all.inference; },
            [&runtime](const auto &context, const auto &value) {
                return runtime.settings().updateInference(context, value);
            },
            [](auto &value) {
                value.samplingSteps = 32;
                value.selectedGpuIndex = 0;
            },
            [](auto &value) { value.executionProvider = QStringLiteral("UnknownProvider"); },
            [](auto &value) { value.cacheDirectory = QStringLiteral("other-cache"); });

        exerciseSettingsCategory<Automation::DeveloperSettingsDto>(
            log, harness, QStringLiteral("SETTINGS-C-DEVELOPER"),
            Automation::OperationIds::settings::update_developer,
            [](const auto &all) { return all.developer; },
            [&runtime](const auto &context, const auto &value) {
                return runtime.settings().updateDeveloper(context, value);
            },
            [](auto &value) { value.enableDiagnostics = true; },
            [](auto &value) {
                value.editorRenderBackend = static_cast<Automation::EditorRenderBackend>(99);
            },
            [](auto &value) { value.showLogWindow = true; });

        exerciseSettingsCategory<Automation::G2pLanguageSettingsDto>(
            log, harness, QStringLiteral("SETTINGS-C-G2P"),
            Automation::OperationIds::settings::update_g2p_language,
            [](const auto &all) { return all.g2pLanguage; },
            [&runtime](const auto &context, const auto &value) {
                return runtime.settings().updateG2pLanguage(context, value);
            },
            [](auto &value) {
                value.languageOrder = {QStringLiteral("cmn"), QStringLiteral("eng")};
            },
            [](auto &value) {
                value.languageOrder = {QStringLiteral("cmn"), QStringLiteral("cmn")};
            },
            [](auto &value) { value.languageOrder.append(QStringLiteral("jpn")); });

        exerciseSettingsCategory<Automation::FillLyricSettingsDto>(
            log, harness, QStringLiteral("SETTINGS-C-FILL-LYRIC"),
            Automation::OperationIds::settings::update_fill_lyric,
            [](const auto &all) { return all.fillLyric; },
            [&runtime](const auto &context, const auto &value) {
                return runtime.settings().updateFillLyric(context, value);
            },
            [](auto &value) {
                value.skipSlur = true;
                value.customSplitterRules.append({
                    .name = QStringLiteral("unicode-分词"),
                    .regexes = {QStringLiteral("[,，]")},
                });
                value.customTaggerRules.append({
                    .language = QStringLiteral("cmn"),
                    .entries = {{
                        .type = QStringLiteral("regex"),
                        .value = {QStringLiteral("^la$")},
                        .tag = QStringLiteral("tag"),
                    }},
                });
            },
            [](auto &value) { value.textEditFontSize = 0.0; },
            [](auto &value) { value.extensionVisible = true; });

        exerciseSettingsCategory<Automation::WindowSettingsDto>(
            log, harness, QStringLiteral("SETTINGS-C-WINDOW"),
            Automation::OperationIds::settings::update_window,
            [](const auto &all) { return all.window; },
            [&runtime](const auto &context, const auto &value) {
                return runtime.settings().updateWindow(context, value);
            },
            [](auto &value) { value.mainWindowGeometry = QByteArrayLiteral("geometry-one"); }, {},
            [](auto &value) { value.mainWindowGeometry = QByteArrayLiteral("geometry-two"); });

        exerciseSettingsCategory<Automation::AudioSettingsDto>(
            log, harness, QStringLiteral("SETTINGS-C-AUDIO"),
            Automation::OperationIds::settings::update_audio,
            [](const auto &all) { return all.audio; },
            [&runtime](const auto &context, const auto &value) {
                return runtime.settings().updateAudio(context, value);
            },
            [](auto &value) {
                value.deviceGain = 0.8;
                value.deviceName = QStringLiteral("Device");
            },
            [](auto &value) { value.devicePan = 2.0; },
            [](auto &value) { value.vstEditorPort = 28083; });

        log.scenario(QStringLiteral("SETTINGS-Q-UPDATED-SNAPSHOT"));
        const auto updated = runtime.settings().getSettings();
        log.expect(updated && updated.get() == harness.settings,
                   QStringLiteral("settings query must reflect committed Unicode-rich values"));

        log.scenario(QStringLiteral("SETTINGS-C-LYRIC-RULE-VALIDATION"));
        const auto writesBefore = harness.settingsWrites;
        auto duplicateSplitter = harness.settings.fillLyric;
        duplicateSplitter.customSplitterRules.append(duplicateSplitter.customSplitterRules.first());
        auto duplicateTagger = harness.settings.fillLyric;
        duplicateTagger.customTaggerRules.append(duplicateTagger.customTaggerRules.first());
        auto invalidTagger = harness.settings.fillLyric;
        invalidTagger.customTaggerRules.first().entries.first().type =
            QStringLiteral("unsupported");
        const auto rejectedSplitter =
            runtime.settings().updateFillLyric(applicationContext(), duplicateSplitter);
        const auto rejectedTagger =
            runtime.settings().updateFillLyric(applicationContext(), duplicateTagger);
        const auto rejectedEntry =
            runtime.settings().updateFillLyric(applicationContext(), invalidTagger);
        log.expectError(rejectedSplitter, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::settings::update_fill_lyric,
                        QStringLiteral("duplicate lyric splitter names must be rejected"));
        log.expectError(rejectedTagger, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::settings::update_fill_lyric,
                        QStringLiteral("duplicate lyric tagger languages must be rejected"));
        log.expectError(rejectedEntry, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::settings::update_fill_lyric,
                        QStringLiteral("unsupported lyric tagger entry type must be rejected"));
        log.expect(harness.settingsWrites == writesBefore,
                   QStringLiteral("invalid lyric rules must not reach persistence"));
    }

    void testRecentFiles(TestLog &log) {
        RuntimeHarness harness;
        auto &runtime = harness.core();

        log.scenario(QStringLiteral("RECENT-Q-EMPTY"));
        const auto initial = runtime.settings().getRecentProjectFiles();
        log.expect(initial && initial.get().isEmpty(),
                   QStringLiteral("recent file query must preserve an empty list"));

        log.scenario(QStringLiteral("RECENT-C-ADD-NORMALIZE"));
        const auto add = runtime.settings().addRecentProjectFile(
            applicationContext(), QStringLiteral(" projects/../projects/歌曲.dspx "));
        const auto duplicate = runtime.settings().addRecentProjectFile(
            applicationContext(), QStringLiteral("projects/歌曲.dspx"));
        const auto added = runtime.settings().getRecentProjectFiles();
        log.expect(add && add.get().changed && duplicate && !duplicate.get().changed && added &&
                       added.get() == QStringList{QStringLiteral("projects/歌曲.dspx")},
                   QStringLiteral("recent add must trim, clean and deduplicate paths"));

        log.scenario(QStringLiteral("RECENT-C-MAXIMUM-ORDER"));
        for (int index = 0; index < 12; ++index) {
            const auto result = runtime.settings().addRecentProjectFile(
                applicationContext(), QStringLiteral("projects/song-%1.dspx").arg(index));
            log.expect(bool(result), QStringLiteral("bulk recent-file setup must succeed"));
        }
        const auto capped = runtime.settings().getRecentProjectFiles();
        log.expect(capped && capped.get().size() == 10 &&
                       capped.get().first() == QStringLiteral("projects/song-11.dspx") &&
                       capped.get().last() == QStringLiteral("projects/song-2.dspx"),
                   QStringLiteral("recent files must keep the ten newest entries in order"));

        log.scenario(QStringLiteral("RECENT-C-REMOVE"));
        const auto remove = runtime.settings().removeRecentProjectFile(
            applicationContext(), QStringLiteral(" projects/song-7.dspx "));
        const auto removeNoOp = runtime.settings().removeRecentProjectFile(
            applicationContext(), QStringLiteral("projects/missing.dspx"));
        log.expect(remove && remove.get().changed && removeNoOp && !removeNoOp.get().changed,
                   QStringLiteral("recent remove must normalize and no-op for a missing path"));

        log.scenario(QStringLiteral("RECENT-C-CLEAR"));
        const auto clearPreview =
            runtime.settings().clearRecentProjectFiles(applicationContext(true));
        const auto clear = runtime.settings().clearRecentProjectFiles(applicationContext());
        const auto clearNoOp = runtime.settings().clearRecentProjectFiles(applicationContext());
        log.expect(clearPreview && clearPreview.get().validatedOnly && clearPreview.get().changed &&
                       clear && clear.get().changed && clearNoOp && !clearNoOp.get().changed &&
                       harness.settings.general.recentProjectFiles.isEmpty(),
                   QStringLiteral("recent clear must preview, commit once and detect empty no-op"));

        log.scenario(QStringLiteral("RECENT-C-INVALID-PATH"));
        const auto invalidAdd =
            runtime.settings().addRecentProjectFile(applicationContext(), QStringLiteral(" "));
        const auto invalidRemove =
            runtime.settings().removeRecentProjectFile(applicationContext(), QStringLiteral(" "));
        log.expect(
            !invalidAdd &&
                invalidAdd.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                !invalidRemove &&
                invalidRemove.getError().code == Automation::AutomationErrorCode::InvalidArgument,
            QStringLiteral("empty recent paths must be rejected without persistence"));

        log.scenario(QStringLiteral("RECENT-C-PERSISTENCE-FAILURE"));
        harness.settingsApplySucceeds = false;
        const auto failed = runtime.settings().addRecentProjectFile(
            applicationContext(), QStringLiteral("projects/failure.dspx"));
        log.expectError(failed, Automation::AutomationErrorCode::IoError,
                        Automation::OperationIds::recent_files::add,
                        QStringLiteral("recent-file persistence failure must be stable"));
        log.expect(harness.settings.general.recentProjectFiles.isEmpty(),
                   QStringLiteral("failed recent add must not alter stored files"));
    }

    void testPackageSearchPaths(TestLog &log) {
        RuntimeHarness harness;
        auto &runtime = harness.core();

        log.scenario(QStringLiteral("PACKAGE-PATH-Q-EMPTY"));
        const auto initial = runtime.settings().getPackageSearchPaths();
        log.expect(initial && initial.get().isEmpty(),
                   QStringLiteral("package path query must preserve an empty list"));

        log.scenario(QStringLiteral("PACKAGE-PATH-C-NORMALIZE"));
        const QStringList input{
            QStringLiteral(" packages/../voices/主声库 "),
            QStringLiteral("voices/主声库"),
            QStringLiteral(" "),
            QStringLiteral("voices/secondary"),
        };
        const auto preview =
            runtime.settings().setPackageSearchPaths(applicationContext(true), input);
        const auto committed =
            runtime.settings().setPackageSearchPaths(applicationContext(), input);
        const auto noOp = runtime.settings().setPackageSearchPaths(
            applicationContext(),
            {QStringLiteral("voices/主声库"), QStringLiteral("voices/secondary")});
        const auto result = runtime.settings().getPackageSearchPaths();
        log.expect(preview && preview.get().validatedOnly && preview.get().changed && committed &&
                       committed.get().changed && noOp && !noOp.get().changed && result &&
                       result.get() == QStringList{QStringLiteral("voices/主声库"),
                                                   QStringLiteral("voices/secondary")},
                   QStringLiteral("package paths must normalize, deduplicate and preserve order"));

        log.scenario(QStringLiteral("PACKAGE-PATH-C-PERSISTENCE-FAILURE"));
        harness.settingsApplySucceeds = false;
        const auto failed = runtime.settings().setPackageSearchPaths(
            applicationContext(), {QStringLiteral("voices/failure")});
        log.expectError(failed, Automation::AutomationErrorCode::IoError,
                        Automation::OperationIds::packages::set_search_paths,
                        QStringLiteral("package path persistence failure must be reported"));
        log.expect(harness.settings.general.packageSearchPaths == result.get(),
                   QStringLiteral("failed package path write must preserve stored paths"));
    }

    void testPackages(TestLog &log) {
        RuntimeHarness harness;
        auto &runtime = harness.core();

        log.scenario(QStringLiteral("PACKAGES-Q-LIST-SNAPSHOT"));
        const auto installed = runtime.packages().getInstalledPackages();
        const auto expected = harness.packages;
        harness.packages.clear();
        log.expect(installed && installed.get() == expected &&
                       installed.get().first().singers.size() == 1,
                   QStringLiteral("installed package query must return a detached typed snapshot"));

        log.scenario(QStringLiteral("PACKAGES-Q-VALIDATE"));
        const auto validated =
            runtime.packages().validatePackage(QStringLiteral("packages/candidate.dspk"));
        log.expect(validated && !validated.get().hasErrors && validated.get().items.size() == 1 &&
                       harness.packageValidationCalls == 1 &&
                       harness.lastValidatedPackagePath ==
                           QStringLiteral("packages/candidate.dspk"),
                   QStringLiteral("package validation must preserve the backend report"));

        log.scenario(QStringLiteral("PACKAGES-Q-VALIDATE-FAILURES"));
        const auto empty = runtime.packages().validatePackage(QStringLiteral(" "));
        harness.packageValidationSucceeds = false;
        const auto backendFailure =
            runtime.packages().validatePackage(QStringLiteral("packages/broken.dspk"));
        log.expectError(empty, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::packages::validate,
                        QStringLiteral("empty package path must be rejected"));
        log.expectError(backendFailure, Automation::AutomationErrorCode::IoError,
                        Automation::OperationIds::packages::validate,
                        QStringLiteral("package validator failure must retain its error"));

        log.scenario(QStringLiteral("PACKAGES-C-RESOLVE-PREVIEW"));
        const auto version = runtime.documentVersion();
        const auto preview =
            runtime.packages().resolveDocumentVoices(commandContext(runtime, true));
        const auto applied = runtime.packages().resolveDocumentVoices(commandContext(runtime));
        log.expect(preview && preview.get().validatedOnly && preview.get().changed && applied &&
                       applied.get().changed && harness.packageResolvePreviewCalls == 1 &&
                       harness.packageResolveApplyCalls == 1 &&
                       runtime.documentVersion() == version,
                   QStringLiteral("voice resolution must preview/apply without document revision"));

        log.scenario(QStringLiteral("PACKAGES-C-RESOLVE-NOOP"));
        harness.packageResolveCount = 0;
        const auto noOp = runtime.packages().resolveDocumentVoices(commandContext(runtime));
        log.expect(noOp && !noOp.get().changed && harness.packageResolveApplyCalls == 2 &&
                       runtime.documentVersion() == version,
                   QStringLiteral("zero resolved voices must be a successful no-op"));

        log.scenario(QStringLiteral("PACKAGES-C-RESOLVE-ERROR-PRIORITY"));
        auto stale = commandContext(runtime);
        ++stale.expected.revision;
        const auto staleResult = runtime.packages().resolveDocumentVoices(stale);
        auto replaced = stale;
        replaced.expected.documentId = Automation::DocumentId::create();
        const auto replacedResult = runtime.packages().resolveDocumentVoices(replaced);
        log.expectError(staleResult, Automation::AutomationErrorCode::RevisionConflict,
                        Automation::OperationIds::packages::resolve_document_voices,
                        QStringLiteral("voice resolution must check revision before its service"));
        log.expectError(replacedResult, Automation::AutomationErrorCode::DocumentChanged,
                        Automation::OperationIds::packages::resolve_document_voices,
                        QStringLiteral("voice resolution must check document before revision"));
        log.expect(harness.packageResolveApplyCalls == 2,
                   QStringLiteral("routing failures must not call voice resolution"));

        log.scenario(QStringLiteral("PACKAGES-C-RESOLVE-IDEMPOTENCY"));
        auto keyed = commandContext(runtime);
        keyed.idempotencyKey = QStringLiteral("package-resolve-key");
        const auto unsupportedKey = runtime.packages().resolveDocumentVoices(keyed);
        log.expectError(unsupportedKey, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::packages::resolve_document_voices,
                        QStringLiteral("cache-only voice resolution must reject idempotency keys"));
    }

    Automation::SpeakerMixPresetDto validPreset(const QString &name = QStringLiteral("Lead")) {
        return {
            .name = name,
            .packageId = QStringLiteral("voice.package"),
            .singerId = QStringLiteral("singer"),
            .packageVersion = QVersionNumber(2, 1),
            .sources =
                {
                          {.speakerId = QStringLiteral("speaker-a"),
                     .speakerName = QStringLiteral("Speaker A")},
                          {.speakerId = QStringLiteral("speaker-b"),
                     .speakerName = QStringLiteral("Speaker B")},
                          },
            .fixedWeights = {0.75, 0.25},
        };
    }

    void testSpeakerMixPresets(TestLog &log) {
        RuntimeHarness harness;
        auto &runtime = harness.core();
        const auto version = runtime.documentVersion();

        log.scenario(QStringLiteral("PRESETS-Q-EMPTY"));
        const auto initial = runtime.presets().getSpeakerMixPresets();
        log.expect(initial && initial.get().isEmpty(),
                   QStringLiteral("preset query must preserve an empty collection"));

        log.scenario(QStringLiteral("PRESETS-C-SAVE-PREVIEW"));
        const auto preview = runtime.presets().saveSpeakerMixPreset(
            applicationContext(true), validPreset(QStringLiteral("主唱")));
        log.expect(preview && preview.get().id.isEmpty() && !preview.get().createdAt.isValid() &&
                       !preview.get().updatedAt.isValid() && harness.presetWriteAttempts == 0 &&
                       harness.presets.isEmpty(),
                   QStringLiteral("preset preview must not allocate IDs, timestamps or storage"));

        log.scenario(QStringLiteral("PRESETS-C-SAVE-COMMIT"));
        const auto saved = runtime.presets().saveSpeakerMixPreset(
            applicationContext(), validPreset(QStringLiteral("主唱")));
        log.expect(saved && !saved.get().id.isEmpty() && saved.get().createdAt.isValid() &&
                       saved.get().updatedAt.isValid() &&
                       harness.presets == QList<Automation::SpeakerMixPresetDto>{saved.get()} &&
                       harness.presetWrites == 1 && runtime.documentVersion() == version,
                   QStringLiteral("preset save must allocate metadata and persist atomically"));

        log.scenario(QStringLiteral("PRESETS-Q-SNAPSHOT"));
        const auto listed = runtime.presets().getSpeakerMixPresets();
        harness.presets.first().name = QStringLiteral("mutated-after-query");
        log.expect(listed && listed.get() == QList<Automation::SpeakerMixPresetDto>{saved.get()},
                   QStringLiteral("preset list must be an owned snapshot"));
        harness.presets = listed.get();

        log.scenario(QStringLiteral("PRESETS-C-UPDATE"));
        auto update = saved.get();
        update.name = QStringLiteral("主唱更新");
        update.fixedWeights = {0.6, 0.4};
        const auto updated = runtime.presets().saveSpeakerMixPreset(applicationContext(), update);
        log.expect(updated && updated.get().id == saved.get().id &&
                       updated.get().createdAt == saved.get().createdAt &&
                       updated.get().name == QStringLiteral("主唱更新") &&
                       harness.presets == QList<Automation::SpeakerMixPresetDto>{updated.get()} &&
                       harness.presetWrites == 2,
                   QStringLiteral("preset update must preserve identity and creation time"));

        log.scenario(QStringLiteral("PRESETS-C-DUPLICATE-NAME"));
        auto duplicate = validPreset(QStringLiteral("主唱更新"));
        duplicate.id = QStringLiteral("different-id");
        const auto duplicateResult =
            runtime.presets().saveSpeakerMixPreset(applicationContext(), duplicate);
        log.expectError(duplicateResult, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::speaker_mix_presets::save,
                        QStringLiteral("same-singer duplicate preset name must be rejected"));

        log.scenario(QStringLiteral("PRESETS-C-VALIDATION"));
        auto emptyName = validPreset();
        emptyName.name.clear();
        auto mismatched = validPreset();
        mismatched.fixedWeights.removeLast();
        auto emptySpeaker = validPreset();
        emptySpeaker.sources.first().speakerId.clear();
        auto nonFinite = validPreset();
        nonFinite.fixedWeights.first() = std::numeric_limits<double>::quiet_NaN();
        const auto invalidName =
            runtime.presets().saveSpeakerMixPreset(applicationContext(), emptyName);
        const auto invalidSize =
            runtime.presets().saveSpeakerMixPreset(applicationContext(), mismatched);
        const auto invalidSpeaker =
            runtime.presets().saveSpeakerMixPreset(applicationContext(), emptySpeaker);
        const auto invalidWeight =
            runtime.presets().saveSpeakerMixPreset(applicationContext(), nonFinite);
        log.expectError(invalidName, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::speaker_mix_presets::save,
                        QStringLiteral("preset identity fields must be required"));
        log.expectError(invalidSize, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::speaker_mix_presets::save,
                        QStringLiteral("preset sources and weights must align"));
        log.expectError(invalidSpeaker, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::speaker_mix_presets::save,
                        QStringLiteral("preset speaker ID must be required"));
        log.expectError(invalidWeight, Automation::AutomationErrorCode::InvalidArgument,
                        Automation::OperationIds::speaker_mix_presets::save,
                        QStringLiteral("preset weight must be finite"));

        log.scenario(QStringLiteral("PRESETS-C-PERSISTENCE-FAILURE"));
        auto failedUpdate = updated.get();
        failedUpdate.name = QStringLiteral("failed update");
        harness.presetApplySucceeds = false;
        const auto failed =
            runtime.presets().saveSpeakerMixPreset(applicationContext(), failedUpdate);
        harness.presetApplySucceeds = true;
        log.expectError(failed, Automation::AutomationErrorCode::IoError,
                        Automation::OperationIds::speaker_mix_presets::save,
                        QStringLiteral("preset save failure must be reported"));
        log.expect(harness.presets == QList<Automation::SpeakerMixPresetDto>{updated.get()} &&
                       harness.presetWrites == 2,
                   QStringLiteral("failed preset save must preserve storage"));

        log.scenario(QStringLiteral("PRESETS-C-DELETE"));
        const auto deleteMissing = runtime.presets().deleteSpeakerMixPreset(
            applicationContext(), QStringLiteral("missing"));
        const auto deletePreview =
            runtime.presets().deleteSpeakerMixPreset(applicationContext(true), updated.get().id);
        log.expect(deleteMissing && !deleteMissing.get().changed && deletePreview &&
                       deletePreview.get().changed && deletePreview.get().validatedOnly &&
                       harness.presets == QList<Automation::SpeakerMixPresetDto>{updated.get()},
                   QStringLiteral("preset delete must no-op for missing and preview existing"));

        harness.presetApplySucceeds = false;
        const auto deleteFailed =
            runtime.presets().deleteSpeakerMixPreset(applicationContext(), updated.get().id);
        harness.presetApplySucceeds = true;
        const auto deleted =
            runtime.presets().deleteSpeakerMixPreset(applicationContext(), updated.get().id);
        const auto deletedAgain =
            runtime.presets().deleteSpeakerMixPreset(applicationContext(), updated.get().id);
        log.expectError(deleteFailed, Automation::AutomationErrorCode::IoError,
                        Automation::OperationIds::speaker_mix_presets::delete_preset,
                        QStringLiteral("preset delete persistence failure must be stable"));
        log.expect(deleted && deleted.get().changed && deletedAgain &&
                       !deletedAgain.get().changed && harness.presets.isEmpty() &&
                       runtime.documentVersion() == version,
                   QStringLiteral("preset delete must commit once and preserve document version"));

        log.scenario(QStringLiteral("PRESETS-C-DELETE-INVALID"));
        const auto invalidDelete =
            runtime.presets().deleteSpeakerMixPreset(applicationContext(), QStringLiteral(" "));
        log.expect(!invalidDelete && invalidDelete.getError().code ==
                                         Automation::AutomationErrorCode::InvalidArgument,
                   QStringLiteral("empty preset ID must be rejected"));
    }

    void testUnavailableHostCapabilities(TestLog &log) {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();

        log.scenario(QStringLiteral("HOST-APPLICATION-UNAVAILABLE"));
        const auto info = runtime.application().getInfo();
        const auto exit = runtime.application().requestTermination(
            guiContext(runtime), Automation::ApplicationTerminationMode::Exit);
        log.expectError(info, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::application::get_info,
                        QStringLiteral("missing application info host must be explicit"));
        log.expectError(exit, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::application::request_exit,
                        QStringLiteral("missing lifecycle host must be explicit"));

        log.scenario(QStringLiteral("HOST-PLAYBACK-UNAVAILABLE"));
        const auto playback = runtime.playback().getPlayback(runtime.documentVersion().documentId);
        const auto play = runtime.playback().play(commandContext(runtime));
        const auto position = runtime.playback().setPosition(commandContext(runtime), 120.0);
        const auto loop =
            runtime.playback().setLoop(commandContext(runtime), LoopSettings(false, 0, 480));
        log.expectError(playback, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::playback::get,
                        QStringLiteral("missing playback snapshot host must be explicit"));
        log.expectError(play, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::playback::play,
                        QStringLiteral("missing playback state host must be explicit"));
        log.expectError(position, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::playback::set_position,
                        QStringLiteral("missing playback position host must be explicit"));
        log.expectError(loop, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::playback::set_loop,
                        QStringLiteral("missing playback loop host must be explicit"));

        log.scenario(QStringLiteral("HOST-EDITOR-UNAVAILABLE"));
        const auto state = runtime.facade().getEditorState(runtime.documentVersion().documentId,
                                                           runtime.windowId());
        const auto center = runtime.facade().centerPianoRoll(guiContext(runtime), 120.0, 60.0);
        const auto quantize = runtime.facade().setPianoRollQuantize(guiContext(runtime), 16, true);
        log.expect(state && !state.get().view,
                   QStringLiteral("editor state remains queryable without an attached view"));
        log.expectError(center, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::editor::center_piano_roll,
                        QStringLiteral("missing editor view host must be explicit"));
        log.expectError(quantize, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::editor::set_quantize,
                        QStringLiteral("missing stable editor host must be explicit"));

        log.scenario(QStringLiteral("HOST-SETTINGS-UNAVAILABLE"));
        const auto settings = runtime.settings().getSettings();
        const auto updateGeneral =
            runtime.settings().updateGeneral(applicationContext(), validSettings().general);
        const auto recent = runtime.settings().getRecentProjectFiles();
        const auto paths = runtime.settings().getPackageSearchPaths();
        log.expectError(settings, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::settings::get,
                        QStringLiteral("missing settings snapshot host must be explicit"));
        log.expectError(updateGeneral, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::settings::update_general,
                        QStringLiteral("missing settings persistence host must be explicit"));
        log.expectError(recent, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::recent_files::list,
                        QStringLiteral("missing recent-file host must be explicit"));
        log.expectError(paths, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::packages::get_search_paths,
                        QStringLiteral("missing package-path host must be explicit"));

        log.scenario(QStringLiteral("HOST-PACKAGES-UNAVAILABLE"));
        const auto packages = runtime.packages().getInstalledPackages();
        const auto validation = runtime.packages().validatePackage(QStringLiteral("package.dspk"));
        const auto resolve = runtime.packages().resolveDocumentVoices(commandContext(runtime));
        log.expectError(packages, Automation::AutomationErrorCode::ModuleNotReady,
                        Automation::OperationIds::packages::list,
                        QStringLiteral("missing package registry must be explicit"));
        log.expectError(validation, Automation::AutomationErrorCode::ModuleNotReady,
                        Automation::OperationIds::packages::validate,
                        QStringLiteral("missing package validator must be explicit"));
        log.expectError(resolve, Automation::AutomationErrorCode::ModuleNotReady,
                        Automation::OperationIds::packages::resolve_document_voices,
                        QStringLiteral("missing voice resolver must be explicit"));

        log.scenario(QStringLiteral("HOST-PRESETS-UNAVAILABLE"));
        const auto presets = runtime.presets().getSpeakerMixPresets();
        const auto save =
            runtime.presets().saveSpeakerMixPreset(applicationContext(), validPreset());
        const auto remove = runtime.presets().deleteSpeakerMixPreset(applicationContext(),
                                                                     QStringLiteral("preset"));
        log.expectError(presets, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::speaker_mix_presets::list,
                        QStringLiteral("missing preset list host must be explicit"));
        log.expectError(save, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::speaker_mix_presets::save,
                        QStringLiteral("missing preset save host must be explicit"));
        log.expectError(remove, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        Automation::OperationIds::speaker_mix_presets::delete_preset,
                        QStringLiteral("missing preset delete host must be explicit"));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    TestLog log;
    testApplicationLifecycle(log);
    testPlaybackHostState(log);
    testEditorViewCommands(log);
    testEditorStateAndSelection(log);
    testSettingsDomains(log);
    testRecentFiles(log);
    testPackageSearchPaths(log);
    testPackages(log);
    testSpeakerMixPresets(log);
    testUnavailableHostCapabilities(log);
    return log.finish();
}
