#pragma once

#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "TestRuntime.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

namespace ApplicationTest {



    inline Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime,
                                                     const bool validateOnly = false) {
        return {
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    inline Automation::GuiCommandContext guiContext(const Automation::CoreRuntime &runtime,
                                                    const bool validateOnly = false) {
        Q_ASSERT(runtime.windowId());
        return {
            .windowId = *runtime.windowId(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    inline Automation::GuiDocumentCommandContext
        guiDocumentContext(const Automation::CoreRuntime &runtime,
                           const bool validateOnly = false) {
        Q_ASSERT(runtime.windowId());
        const auto version = runtime.documentVersion();
        return {
            .documentId = version.documentId,
            .expectedRevision = version.revision,
            .windowId = *runtime.windowId(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    inline Automation::ApplicationCommandContext
        applicationContext(const bool validateOnly = false) {
        return {
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    inline Automation::SettingsSnapshotDto validSettings() {
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

    class ApplicationHarness final {
    public:
        explicit ApplicationHarness(
            std::optional<Automation::WindowId> windowId = Automation::WindowId::create())
            : history(HistoryManager::instance()), settings(validSettings()) {
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
                applicationServices(), std::move(windowId));
        }

        ~ApplicationHarness() {
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
        int lyricWrites = 0;
        int lyricTestCalls = 0;
        QString lastLyricTestText;
        int lyricValidationCalls = 0;
        std::optional<Automation::AutomationError> lyricValidationError;
        QList<Automation::LyricRuleDto> lyricRulesSnapshot;
        int refreshStarts = 0;

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
            .buildId = QStringLiteral("test-build"),
        };
        Automation::ApplicationTerminationRequestResult terminationResult =
            Automation::ApplicationTerminationRequestResult::Accepted;
        int terminationCalls = 0;
        Automation::ApplicationTerminationMode lastTerminationMode =
            Automation::ApplicationTerminationMode::Exit;
        Automation::ApplicationTerminationSavePolicy lastTerminationSavePolicy =
            Automation::ApplicationTerminationSavePolicy::RejectUnsaved;

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
            services.setSelectedClips = [this](const QList<int> &ids, const int primaryId) {
                ++editorStableApplyCalls;
                editorStable.selectedClipIds = ids;
                editorStable.primaryClipId = primaryId;
            };
            services.setSelectedNotes = [this](const int clipId, const QList<int> &ids,
                                               const int primaryId) {
                ++editorStableApplyCalls;
                editorStable.activeClipId = clipId;
                editorStable.selectedNoteIds = ids;
                editorStable.primaryNoteId = primaryId;
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
            services.focusVisibility = [](const HistoryFocus &) {
                return HistoryFocusVisibility::ScrollRequired;
            };
            services.revealFocus = [this](const HistoryFocus &focus, const bool) {
                ++revealCalls;
                if (!editorRevealSucceeds)
                    return false;
                if (focus.kind == HistoryFocusKind::TrackClips) {
                    editorView.layout.trackPanelVisible = true;
                    editorView.layout.activeRegion = EditorViewGlobal::Region::TrackPanel;
                    editorView.layout.focusedRegion = EditorViewGlobal::Region::TrackPanel;
                    editorView.trackPanel.centerTick = (focus.tickStart + focus.tickEnd) * 0.5;
                } else {
                    editorView.layout.bottomPanelVisible = true;
                    editorView.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                    editorView.layout.pianoRollVisible = true;
                    editorView.layout.activeRegion = EditorViewGlobal::Region::PianoRoll;
                    editorView.layout.focusedRegion = EditorViewGlobal::Region::PianoRoll;
                    editorView.pianoRoll.centerTick = (focus.tickStart + focus.tickEnd) * 0.5;
                    editorStable.activeClipId = focus.containerId;
                }
                return true;
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
                if constexpr (std::is_same_v<T, Automation::FillLyricSettingsDto>)
                    ++lyricWrites;
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
            services.validateFillLyricRuntime = [this](const Automation::FillLyricSettingsDto &) {
                ++lyricValidationCalls;
                if (lyricValidationError)
                    return Automation::AutomationResult<Automation::AutomationUnit>(
                        *lyricValidationError);
                return Automation::AutomationResult<Automation::AutomationUnit>(
                    Automation::AutomationUnit{});
            };
            services.lyricRules = [this] { return lyricRulesSnapshot; };
            services.testLyricRules = [this](const QString &text) {
                ++lyricTestCalls;
                lastLyricTestText = text;
                return Automation::AutomationResult<Automation::LyricRuleTestResultDto>({
                    .splitTokens = {text},
                    .taggedTokens = {{.lyric = text,
                                      .language = QStringLiteral("unknown"),
                                      .tag = QStringLiteral("unknown")}},
                });
            };
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
            services.refreshPackages = [this](Automation::PackageRefreshCommitGate commitGate,
                                              Automation::PackageRefreshCompletion completion) {
                ++refreshStarts;
                if (commitGate && !commitGate())
                    return Automation::AutomationResult<Automation::AutomationUnit>(
                        Automation::AutomationUnit{});
                completion(Automation::PackageRefreshResultDto{
                    .packages = static_cast<int>(packages.size()),
                    .added = {QStringLiteral("new.package@1.0")},
                    .failures = {{.path = QStringLiteral("private/broken"),
                                  .reason = QStringLiteral("broken")}},
                });
                return Automation::AutomationResult<Automation::AutomationUnit>(
                    Automation::AutomationUnit{});
            };
            return services;
        }

        Automation::ApplicationRuntimeServices applicationServices() {
            Automation::ApplicationRuntimeServices services;
            services.info = [this] { return applicationInfo; };
            services.requestTermination = [this](const auto mode, const auto savePolicy) {
                ++terminationCalls;
                lastTerminationMode = mode;
                lastTerminationSavePolicy = savePolicy;
                return terminationResult;
            };
            return services;
        }

        AppModel model;
        HistoryManager *history;
        std::unique_ptr<Automation::CoreRuntime> runtime;
    };

    inline Automation::SpeakerMixPresetDto
        validPreset(const QString &name = QStringLiteral("Lead")) {
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
            .fixedWeights = {0.75                                     },
        };
    }
}
