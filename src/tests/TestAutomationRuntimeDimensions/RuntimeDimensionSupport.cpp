#include "RuntimeDimensionSupport.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QDateTime>
#include <QRegularExpression>
#include <QVersionNumber>

#include <utility>

namespace RuntimeDimensions {
    namespace {
        QString scenarioId(const Automation::OperationId &operationId, const QString &dimension) {
            auto operation = operationId.toUpper();
            operation.replace(QRegularExpression(QStringLiteral("[^A-Z0-9]+")),
                              QStringLiteral("-"));
            return QStringLiteral("AFD-%1-%2").arg(operation, dimension.toUpper());
        }

        CoverageDimensions classifyDimension(const QString &dimension) {
            const auto value = dimension.toUpper();
            const auto containsAny = [&value](std::initializer_list<QStringView> tokens) {
                for (const auto token : tokens) {
                    if (value.contains(token))
                        return true;
                }
                return false;
            };

            CoverageDimensions result = 0;
            if (value.startsWith(QStringLiteral("NORMAL")) ||
                value.startsWith(QStringLiteral("TYPED")) ||
                containsAny({u"COMMIT", u"MINIMAL", u"RICH-SNAPSHOT", u"ALL-CATEGORIES",
                             u"ORDERED-UNICODE", u"REPORT", u"CREATE-UNICODE", u"UPDATE",
                             u"TRACK-PANEL", u"PIANO-ROLL", u"PIANO-NOTES", u"TRACK-CLIPS",
                             u"OPERATION-SURFACE", u"LIMITS"}) ||
                value == QStringLiteral("EMPTY")) {
                result |= NormalData;
            }
            if (containsAny({u"SNAPSHOT", u"NO-SIDE-EFFECT", u"DETERMINISTIC", u"REPEATED-QUERY",
                             u"NO-HOST-DEPENDENCY", u"OPTIONAL-VIEW"})) {
                result |= SnapshotNoSideEffect;
            }
            if (containsAny({u"VALIDATE", u"NO-OP"}))
                result |= ValidateOnlyOrNoOp;
            if (containsAny({u"UNKNOWN-WINDOW", u"UNKNOWN-DOCUMENT", u"STALE-REVISION",
                             u"MISSING-CLIP", u"MISSING-OBJECT"})) {
                result |= UnknownTargetOrRevision;
            }
            if (containsAny({u"HOST-UNAVAILABLE", u"HOST-REJECT", u"HOST-FAILURES",
                             u"DEVICE-FAILURE", u"BACKEND-FAILURE", u"OPTIONAL-HOST"})) {
                result |= HostUnavailable;
            }
            if (containsAny({u"UNICODE", u"INVALID", u"BOUNDAR", u"NON-FINITE", u"NEGATIVE",
                             u"EMPTY", u"DUPLICATE", u"MAXIMUM", u"CASE-FOLD", u"LIMITS",
                             u"RANGE-FALLBACK", u"BUSY", u"MISSING-CLIP", u"MISSING-OBJECT"})) {
                result |= BoundaryOrUnicode;
            }
            if (containsAny({u"PERSISTENCE-FAILURE", u"SINGLE-HOST-CALL", u"HOST-REJECT",
                             u"HOST-FAILURES", u"DEVICE-FAILURE", u"BACKEND-FAILURE"})) {
                result |= PersistenceOrSingleHost;
            }
            if (value == QStringLiteral("NORMAL-HISTORY"))
                result |= PersistenceOrSingleHost;
            if (containsAny({u"IDEMPOTEN", u"REPEATED-REQUEST"}))
                result |= IdempotencyOrRepeat;
            return result;
        }

        QStringList dimensionNames(const CoverageDimensions dimensions) {
            QStringList result;
            const std::initializer_list<std::pair<CoverageDimension, QStringView>> names = {
                {NormalData,              u"normal/data"            },
                {SnapshotNoSideEffect,    u"snapshot/no-side-effect"},
                {ValidateOnlyOrNoOp,      u"validate-only/no-op"    },
                {UnknownTargetOrRevision, u"unknown-target/revision"},
                {HostUnavailable,         u"host-unavailable"       },
                {BoundaryOrUnicode,       u"boundary/Unicode"       },
                {PersistenceOrSingleHost, u"persistence/single-host"},
                {IdempotencyOrRepeat,     u"idempotency/repeat"     },
            };
            for (const auto &[flag, name] : names) {
                if (dimensions & flag)
                    result.append(name.toString());
            }
            return result;
        }
    }

    void ScenarioLog::begin(const Automation::OperationId &operationId, const QString &dimension) {
        m_currentScenario = scenarioId(operationId, dimension);
        ++m_scenarios;
        ++m_operationScenarios[operationId];
        m_operationDimensions[operationId] |= classifyDimension(dimension);
        if (m_scenarioIds.contains(m_currentScenario)) {
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_currentScenario << "]: duplicate scenario ID"
                                << Qt::endl;
        }
        m_scenarioIds.insert(m_currentScenario);
    }

    bool ScenarioLog::expect(const bool condition, const QString &message) {
        ++m_assertions;
        if (condition)
            return true;
        ++m_failures;
        QTextStream(stderr) << "FAILED [" << m_currentScenario << "]: " << message << Qt::endl;
        return false;
    }

    int ScenarioLog::finish(const RuntimeDimensionMatrix &expectedDimensions) {
        const auto expectedOperations = expectedDimensions.keys();
        const QSet<QString> expected(expectedOperations.cbegin(), expectedOperations.cend());
        const QSet<QString> actual(m_operationScenarios.keyBegin(), m_operationScenarios.keyEnd());
        ++m_assertions;
        if (actual != expected) {
            ++m_failures;
            QTextStream(stderr) << "FAILED [AFD-MATRIX-OPERATIONS]: expected " << expected.size()
                                << " operations, observed " << actual.size() << Qt::endl;
        }
        for (const auto &operationId : expectedOperations) {
            ++m_assertions;
            const auto count = m_operationScenarios.value(operationId);
            if (count < 6 || count > 9) {
                ++m_failures;
                QTextStream(stderr) << "FAILED [AFD-MATRIX-DIMENSIONS]: " << operationId << " has "
                                    << count << " scenarios; expected 6..9" << Qt::endl;
            }
            ++m_assertions;
            const auto missing =
                expectedDimensions.value(operationId) & ~m_operationDimensions.value(operationId);
            if (missing != 0) {
                ++m_failures;
                QTextStream(stderr)
                    << "FAILED [AFD-MATRIX-APPLICABILITY]: " << operationId << " missing "
                    << dimensionNames(missing).join(QStringLiteral(", ")) << Qt::endl;
            }
        }
        QTextStream(stdout) << "Automation runtime dimension coverage: " << m_scenarios
                            << " scenarios across " << actual.size() << " operations, "
                            << m_assertions << " assertions, " << m_failures << " failures"
                            << Qt::endl;
        for (auto it = m_operationScenarios.cbegin(); it != m_operationScenarios.cend(); ++it)
            QTextStream(stdout) << "  " << it.key() << ": " << it.value() << Qt::endl;
        return m_failures == 0 ? 0 : 1;
    }

    Automation::SettingsSnapshotDto validSettings() {
        Automation::SettingsSnapshotDto result;
        result.general.uiLanguage = QStringLiteral("system");
        result.general.defaultSingingLanguage = QStringLiteral("cmn");
        result.general.defaultLyrics.insert(QStringLiteral("cmn"), QStringLiteral("啦"));
        result.appearance.themeId = QStringLiteral("system");
        result.inference.executionProvider = QStringLiteral("CPU");
        result.inference.cacheDirectory = QStringLiteral("cache");
        for (int index = 0; index < 4; ++index) {
            result.audio.pseudoSingerSynthesizers.append({
                .generator = index,
                .amplitude = -12.0,
                .attackMilliseconds = 10,
                .decayMilliseconds = 1000,
                .decayRatio = 0.5,
                .releaseMilliseconds = 50,
            });
        }
        return result;
    }

    EditorViewState validViewState() {
        EditorViewState result;
        result.trackPanel.centerTick = 120.0;
        result.trackPanel.centerTrackIndex = 1.0;
        result.trackPanel.horizontalScale = 1.0;
        result.trackPanel.verticalScale = 1.0;
        result.layout.trackPanelVisible = true;
        result.layout.bottomPanelVisible = true;
        result.layout.bottomPanelPageId = QStringLiteral("Parameters");
        result.pianoRoll.centerTick = 240.0;
        result.pianoRoll.centerKeyIndex = 60.0;
        result.pianoRoll.horizontalScale = 1.0;
        result.pianoRoll.verticalScale = 1.0;
        result.pianoRoll.editMode = EditorViewGlobal::Select;
        return result;
    }

    Automation::SpeakerMixPresetDto validPreset(const QString &name) {
        return {
            .name = name,
            .packageId = QStringLiteral("voice.package"),
            .singerId = QStringLiteral("singer.主唱"),
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

    HistoryManager *RuntimeHarness::resetGlobalHistory() {
        auto *history = HistoryManager::instance();
        history->reset();
        return history;
    }

    RuntimeHarness::RuntimeHarness(HarnessOptions options)
        : applicationInfo{
              .name = QStringLiteral("DS Editor Lite 测试"),
              .version = QStringLiteral("1.0-test"),
              .platform = QStringLiteral("Windows 测试"),
              .buildId = QStringLiteral("test-build"),
          },
          editorView(validViewState()), settings(validSettings()), m_options(std::move(options)),
          m_history(resetGlobalHistory()) {
        packages.append({
            .id = QStringLiteral("voice.package.主唱"),
            .version = QVersionNumber(2, 1),
            .vendor = QStringLiteral("OpenVPI 测试"),
            .description = QStringLiteral("Unicode package snapshot"),
            .license = QStringLiteral("Test"),
            .readme = QStringLiteral("说明"),
            .url = QStringLiteral("https://example.invalid/package"),
            .path = QStringLiteral("packages/主声库.dspk"),
            .singers = {{
                .singerId = QStringLiteral("singer.主唱"),
                .packageId = QStringLiteral("voice.package.主唱"),
                .packageVersion = QVersionNumber(2, 1),
                .name = QStringLiteral("主唱"),
            }},
        });
        packageReport.items.append({
            .severity = Automation::PackageValidationSeverity::Warning,
            .path = QStringLiteral("manifest.yaml"),
            .message = QStringLiteral("测试警告"),
            .actualValue = QStringLiteral("值"),
            .recommendation = QStringLiteral("建议"),
        });
        m_runtime = std::make_unique<Automation::CoreRuntime>(
            &m_model, m_history, Automation::DocumentRuntimeServices{}, playbackServices(),
            editorServices(), settingsServices(), presetServices(), packageServices(),
            Automation::InferenceRuntimeServices{}, Automation::FileRuntimeServices{},
            Automation::AudioExportRuntimeServices{}, Automation::ExtractionRuntimeServices{},
            applicationServices());
    }

    RuntimeHarness::~RuntimeHarness() {
        m_runtime.reset();
        m_history->reset();
    }

    Automation::CoreRuntime &RuntimeHarness::core() {
        return *m_runtime;
    }

    const Automation::CoreRuntime &RuntimeHarness::core() const {
        return *m_runtime;
    }

    void RuntimeHarness::resetHistory() {
        m_history->reset();
    }

    bool RuntimeHarness::missing(const Automation::OperationId &operationId) const {
        return m_options.missingOperation == operationId;
    }

    Automation::ApplicationRuntimeServices RuntimeHarness::applicationServices() {
        Automation::ApplicationRuntimeServices services;
        if (!missing(Automation::OperationIds::application::get_info)) {
            services.info = [this] {
                ++hostCalls[Automation::OperationIds::application::get_info];
                return applicationInfo;
            };
        }
        if (!missing(Automation::OperationIds::application::request_exit) &&
            !missing(Automation::OperationIds::application::request_restart)) {
            services.requestTermination = [this](const auto mode) {
                const auto operationId =
                    mode == Automation::ApplicationTerminationMode::Exit
                        ? Automation::OperationIds::application::request_exit
                        : Automation::OperationIds::application::request_restart;
                ++hostCalls[operationId];
                lastTerminationMode = mode;
                return applicationTerminationSucceeds;
            };
        }
        return services;
    }

    Automation::PlaybackRuntimeServices RuntimeHarness::playbackServices() {
        Automation::PlaybackRuntimeServices services;
        if (!missing(Automation::OperationIds::playback::get)) {
            services.snapshot = [this] {
                ++hostCalls[QStringLiteral("playback.snapshot")];
                return playback;
            };
        }
        if (!missing(Automation::OperationIds::playback::play)) {
            services.canStart = [this] { return playbackCanStart; };
            services.play = [this] {
                ++hostCalls[Automation::OperationIds::playback::play];
                if (!playbackPlaySucceeds)
                    return false;
                playback.state = Automation::PlaybackState::Playing;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::playback::pause)) {
            services.pause = [this] {
                ++hostCalls[Automation::OperationIds::playback::pause];
                playback.state = Automation::PlaybackState::Paused;
            };
        }
        if (!missing(Automation::OperationIds::playback::stop)) {
            services.stop = [this] {
                ++hostCalls[Automation::OperationIds::playback::stop];
                playback.state = Automation::PlaybackState::Stopped;
            };
        }
        if (!missing(Automation::OperationIds::playback::set_position)) {
            services.setPosition = [this](const double tick) {
                ++hostCalls[Automation::OperationIds::playback::set_position];
                playback.position = tick;
            };
        }
        if (!missing(Automation::OperationIds::playback::set_last_position)) {
            services.setLastPosition = [this](const double tick) {
                ++hostCalls[Automation::OperationIds::playback::set_last_position];
                playback.lastPosition = tick;
            };
        }
        if (!missing(Automation::OperationIds::playback::set_loop) &&
            !missing(Automation::OperationIds::playback::set_loop_enabled) &&
            !missing(Automation::OperationIds::playback::clear_loop)) {
            services.setLoop = [this](const LoopSettings &value) {
                ++hostCalls[QStringLiteral("playback.loop.apply")];
                playback.loop = value;
            };
        }
        return services;
    }

    Automation::EditorRuntimeServices RuntimeHarness::editorServices() {
        Automation::EditorRuntimeServices services;
        if (!missing(Automation::OperationIds::editor::get_state)) {
            services.captureView = [this]() -> std::optional<EditorViewState> {
                ++hostCalls[QStringLiteral("editor.capture_view")];
                if (!editorViewAvailable)
                    return std::nullopt;
                return editorView;
            };
            services.captureStableState = [this] {
                ++hostCalls[QStringLiteral("editor.capture_stable")];
                return editorStable;
            };
        }
        if (!missing(Automation::OperationIds::editor::restore_view)) {
            services.restoreView = [this](const EditorViewState &value) {
                ++hostCalls[Automation::OperationIds::editor::restore_view];
                if (!editorApplySucceeds)
                    return false;
                editorView = value;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::center_track_panel)) {
            services.centerTrackPanel = [this](const double tick, const double index) {
                ++hostCalls[Automation::OperationIds::editor::center_track_panel];
                if (!editorApplySucceeds)
                    return false;
                editorView.trackPanel.centerTick = tick;
                editorView.trackPanel.centerTrackIndex = index;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_track_panel_scale)) {
            services.setTrackPanelScale = [this](const double horizontal, const double vertical) {
                ++hostCalls[Automation::OperationIds::editor::set_track_panel_scale];
                if (!editorApplySucceeds)
                    return false;
                editorView.trackPanel.horizontalScale = horizontal;
                editorView.trackPanel.verticalScale = vertical;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_track_panel_viewport)) {
            services.setTrackPanelViewport = [this](const TrackPanelViewState &value) {
                ++hostCalls[Automation::OperationIds::editor::set_track_panel_viewport];
                if (!editorApplySucceeds)
                    return false;
                editorView.trackPanel = value;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_panel_visibility)) {
            services.setPanelVisibility = [this](const bool track, const bool bottom) {
                ++hostCalls[Automation::OperationIds::editor::set_panel_visibility];
                if (!editorApplySucceeds)
                    return false;
                editorView.layout.trackPanelVisible = track;
                editorView.layout.bottomPanelVisible = bottom;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::show_bottom_panel_page)) {
            services.showBottomPanelPage = [this](const QString &pageId) {
                ++hostCalls[Automation::OperationIds::editor::show_bottom_panel_page];
                if (!editorApplySucceeds)
                    return false;
                editorView.layout.bottomPanelPageId = pageId;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::show_region)) {
            services.showRegion = [this](const EditorViewGlobal::Region region) {
                ++hostCalls[Automation::OperationIds::editor::show_region];
                if (!editorApplySucceeds)
                    return false;
                editorView.layout.bottomPanelVisible = true;
                editorView.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                if (region == EditorViewGlobal::Region::PianoRoll)
                    editorView.layout.pianoRollVisible = true;
                else
                    editorView.layout.parametersVisible = true;
                editorView.layout.activeRegion = region;
                editorView.layout.focusedRegion = region;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::focus_region)) {
            services.focusRegion = [this](const EditorViewGlobal::Region region) {
                ++hostCalls[Automation::OperationIds::editor::focus_region];
                if (!editorApplySucceeds)
                    return false;
                if (region == EditorViewGlobal::Region::TrackPanel) {
                    editorView.layout.trackPanelVisible = true;
                } else {
                    editorView.layout.bottomPanelVisible = true;
                    editorView.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                    if (region == EditorViewGlobal::Region::PianoRoll)
                        editorView.layout.pianoRollVisible = true;
                    else
                        editorView.layout.parametersVisible = true;
                }
                editorView.layout.activeRegion = region;
                editorView.layout.focusedRegion = region;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::center_piano_roll)) {
            services.centerPianoRoll = [this](const double tick, const double key) {
                ++hostCalls[Automation::OperationIds::editor::center_piano_roll];
                if (!editorApplySucceeds)
                    return false;
                editorView.pianoRoll.centerTick = tick;
                editorView.pianoRoll.centerKeyIndex = key;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_piano_roll_scale)) {
            services.setPianoRollScale = [this](const double horizontal, const double vertical) {
                ++hostCalls[Automation::OperationIds::editor::set_piano_roll_scale];
                if (!editorApplySucceeds)
                    return false;
                editorView.pianoRoll.horizontalScale = horizontal;
                editorView.pianoRoll.verticalScale = vertical;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_clip_editor_time_viewport)) {
            services.setClipEditorTimeViewport = [this](const double tick,
                                                        const double horizontal) {
                ++hostCalls[Automation::OperationIds::editor::set_clip_editor_time_viewport];
                if (!editorApplySucceeds)
                    return false;
                editorView.pianoRoll.centerTick = tick;
                editorView.pianoRoll.horizontalScale = horizontal;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_piano_roll_pitch_viewport)) {
            services.setPianoRollPitchViewport = [this](const double key, const double vertical) {
                ++hostCalls[Automation::OperationIds::editor::set_piano_roll_pitch_viewport];
                if (!editorApplySucceeds)
                    return false;
                editorView.pianoRoll.centerKeyIndex = key;
                editorView.pianoRoll.verticalScale = vertical;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_piano_roll_edit_mode)) {
            services.setPianoRollEditMode = [this](const auto mode) {
                ++hostCalls[Automation::OperationIds::editor::set_piano_roll_edit_mode];
                if (!editorApplySucceeds)
                    return false;
                editorView.pianoRoll.editMode = mode;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_parameter_foreground)) {
            services.setParameterForeground = [this](const ParamInfo::Name name) {
                ++hostCalls[Automation::OperationIds::editor::set_parameter_foreground];
                if (!editorApplySucceeds)
                    return false;
                editorView.parameters.foreground = name;
                editorView.layout.bottomPanelVisible = true;
                editorView.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                editorView.layout.parametersVisible = true;
                editorView.layout.activeRegion = EditorViewGlobal::Region::Parameters;
                editorView.layout.focusedRegion = EditorViewGlobal::Region::Parameters;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_parameter_background)) {
            services.setParameterBackground = [this](const ParamInfo::Name name) {
                ++hostCalls[Automation::OperationIds::editor::set_parameter_background];
                if (!editorApplySucceeds)
                    return false;
                editorView.parameters.background = name;
                editorView.layout.bottomPanelVisible = true;
                editorView.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                editorView.layout.parametersVisible = true;
                editorView.layout.activeRegion = EditorViewGlobal::Region::Parameters;
                editorView.layout.focusedRegion = EditorViewGlobal::Region::Parameters;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::swap_parameters)) {
            services.swapParameters = [this] {
                ++hostCalls[Automation::OperationIds::editor::swap_parameters];
                if (!editorApplySucceeds)
                    return false;
                std::swap(editorView.parameters.foreground, editorView.parameters.background);
                editorView.layout.bottomPanelVisible = true;
                editorView.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                editorView.layout.parametersVisible = true;
                editorView.layout.activeRegion = EditorViewGlobal::Region::Parameters;
                editorView.layout.focusedRegion = EditorViewGlobal::Region::Parameters;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_parameter_edit_mode)) {
            services.setParameterEditMode = [this](const auto mode) {
                ++hostCalls[Automation::OperationIds::editor::set_parameter_edit_mode];
                if (!editorApplySucceeds)
                    return false;
                editorView.parameters.editMode = mode;
                editorView.layout.bottomPanelVisible = true;
                editorView.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                editorView.layout.parametersVisible = true;
                editorView.layout.activeRegion = EditorViewGlobal::Region::Parameters;
                editorView.layout.focusedRegion = EditorViewGlobal::Region::Parameters;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_parameter_value_viewport)) {
            services.setParameterValueViewport = [this](const double center,
                                                        const double vertical) {
                ++hostCalls[Automation::OperationIds::editor::set_parameter_value_viewport];
                if (!editorApplySucceeds)
                    return false;
                editorView.parameters.centerRatio = center;
                editorView.parameters.verticalScale = vertical;
                editorView.layout.bottomPanelVisible = true;
                editorView.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                editorView.layout.parametersVisible = true;
                editorView.layout.activeRegion = EditorViewGlobal::Region::Parameters;
                editorView.layout.focusedRegion = EditorViewGlobal::Region::Parameters;
                return true;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_active_clip)) {
            services.setActiveClip = [this](const int clipId) {
                ++hostCalls[Automation::OperationIds::editor::set_active_clip];
                if (editorStable.activeClipId != clipId)
                    editorStable.selectedNoteIds.clear();
                editorStable.activeClipId = clipId;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_selection)) {
            services.setSelectedTrackIndex = [this](const int index) {
                ++hostCalls[Automation::OperationIds::editor::set_selection];
                editorStable.selectedTrackIndex = index;
            };
            services.setSelectedClips = [this](const QList<int> &ids, const int primaryId) {
                ++hostCalls[Automation::OperationIds::editor::set_selection];
                editorStable.selectedClipIds = ids;
                editorStable.primaryClipId = primaryId;
            };
            services.setSelectedNotes = [this](const int clipId, const QList<int> &ids,
                                               const int primaryId) {
                ++hostCalls[Automation::OperationIds::editor::set_selection];
                editorStable.activeClipId = clipId;
                editorStable.selectedNoteIds = ids;
                editorStable.primaryNoteId = primaryId;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_quantize)) {
            services.setPianoRollQuantize = [this](const int quantize, const bool enabled) {
                ++hostCalls[Automation::OperationIds::editor::set_quantize];
                editorStable.pianoRollQuantize = quantize;
                editorStable.pianoRollQuantizeEnabled = enabled;
            };
        }
        if (!missing(Automation::OperationIds::editor::set_auto_page_turn)) {
            services.setAutoPageTurn = [this](const auto target, const bool enabled) {
                ++hostCalls[Automation::OperationIds::editor::set_auto_page_turn];
                if (target == Automation::EditorAutoPageTarget::TrackPanel)
                    editorStable.trackAutoPageTurnEnabled = enabled;
                else
                    editorStable.pianoRollAutoPageTurnEnabled = enabled;
            };
        }
        if (!missing(Automation::OperationIds::editor::reveal)) {
            services.focusVisibility = [this](const HistoryFocus &) {
                return editorFocusVisibility;
            };
            services.revealFocus = [this](const HistoryFocus &focus, const bool) {
                ++hostCalls[Automation::OperationIds::editor::reveal];
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
                    editorView.pianoRoll.centerKeyIndex = (focus.valueStart + focus.valueEnd) * 0.5;
                    editorStable.activeClipId = focus.containerId;
                }
                editorFocusVisibility = HistoryFocusVisibility::Visible;
                return true;
            };
        }
        return services;
    }

    Automation::SettingsRuntimeServices RuntimeHarness::settingsServices() {
        Automation::SettingsRuntimeServices services;
        const bool missingSnapshot = missing(Automation::OperationIds::settings::get) ||
                                     missing(Automation::OperationIds::recent_files::list) ||
                                     missing(Automation::OperationIds::packages::get_search_paths);
        if (!missingSnapshot) {
            services.snapshot = [this] {
                ++hostCalls[QStringLiteral("settings.snapshot")];
                return settings;
            };
        }
        const bool missingGeneral = missing(Automation::OperationIds::settings::update_general) ||
                                    missing(Automation::OperationIds::recent_files::add) ||
                                    missing(Automation::OperationIds::recent_files::remove) ||
                                    missing(Automation::OperationIds::recent_files::clear) ||
                                    missing(Automation::OperationIds::packages::set_search_paths);
        if (!missingGeneral) {
            services.applyGeneral = [this](const auto &value) {
                return persistSetting(Automation::OperationIds::settings::update_general,
                                      settings.general, value);
            };
        }
        if (!missing(Automation::OperationIds::settings::update_appearance)) {
            services.applyAppearance = [this](const auto &value) {
                return persistSetting(Automation::OperationIds::settings::update_appearance,
                                      settings.appearance, value);
            };
        }
        if (!missing(Automation::OperationIds::settings::update_inference)) {
            services.applyInference = [this](const auto &value) {
                return persistSetting(Automation::OperationIds::settings::update_inference,
                                      settings.inference, value);
            };
        }
        if (!missing(Automation::OperationIds::settings::update_developer)) {
            services.applyDeveloper = [this](const auto &value) {
                return persistSetting(Automation::OperationIds::settings::update_developer,
                                      settings.developer, value);
            };
        }
        if (!missing(Automation::OperationIds::settings::update_g2p_language)) {
            services.applyG2pLanguage = [this](const auto &value) {
                return persistSetting(Automation::OperationIds::settings::update_g2p_language,
                                      settings.g2pLanguage, value);
            };
        }
        if (!missing(Automation::OperationIds::settings::update_fill_lyric)) {
            services.applyFillLyric = [this](const auto &value) {
                return persistSetting(Automation::OperationIds::settings::update_fill_lyric,
                                      settings.fillLyric, value);
            };
        }
        if (!missing(Automation::OperationIds::settings::update_window)) {
            services.applyWindow = [this](const auto &value) {
                return persistSetting(Automation::OperationIds::settings::update_window,
                                      settings.window, value);
            };
        }
        if (!missing(Automation::OperationIds::settings::update_audio)) {
            services.applyAudio = [this](const auto &value) {
                return persistSetting(Automation::OperationIds::settings::update_audio,
                                      settings.audio, value);
            };
        }
        return services;
    }

    Automation::PresetRuntimeServices RuntimeHarness::presetServices() {
        Automation::PresetRuntimeServices services;
        if (!missing(Automation::OperationIds::speaker_mix_presets::list)) {
            services.speakerMixPresets = [this] {
                ++hostCalls[Automation::OperationIds::speaker_mix_presets::list];
                return presets;
            };
        }
        if (!missing(Automation::OperationIds::speaker_mix_presets::save) &&
            !missing(Automation::OperationIds::speaker_mix_presets::delete_preset)) {
            services.applySpeakerMixPresets = [this](const auto &value) {
                ++presetWriteAttempts;
                if (!presetApplySucceeds)
                    return false;
                presets = value;
                ++presetWrites;
                return true;
            };
        }
        return services;
    }

    Automation::PackageRuntimeServices RuntimeHarness::packageServices() {
        Automation::PackageRuntimeServices services;
        if (!missing(Automation::OperationIds::packages::list)) {
            services.installedPackages = [this] {
                ++hostCalls[Automation::OperationIds::packages::list];
                return packages;
            };
        }
        if (!missing(Automation::OperationIds::packages::validate)) {
            services.validatePackage = [this](const QString &path) {
                ++hostCalls[Automation::OperationIds::packages::validate];
                lastValidatedPackagePath = path;
                if (packageValidationSucceeds)
                    return Automation::AutomationResult<Automation::PackageValidationReportDto>(
                        packageReport);
                Automation::AutomationError error;
                error.code = Automation::AutomationErrorCode::IoError;
                error.message = QStringLiteral("simulated package validation failure");
                return Automation::AutomationResult<Automation::PackageValidationReportDto>(
                    std::move(error));
            };
        }
        if (!missing(Automation::OperationIds::packages::resolve_document_voices)) {
            services.resolveDocumentVoices = [this](AppModel *, const bool apply) {
                ++hostCalls[Automation::OperationIds::packages::resolve_document_voices];
                if (apply)
                    ++packageResolveApplyCalls;
                else
                    ++packageResolvePreviewCalls;
                return packageResolveCount;
            };
        }
        return services;
    }

    Automation::CommandContext commandContext(const RuntimeHarness &harness,
                                              const bool validateOnly,
                                              const QString &idempotencyKey) {
        return {
            .expected = harness.core().documentVersion(),
            .validateOnly = validateOnly,
            .idempotencyKey = idempotencyKey,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::GuiCommandContext guiContext(const RuntimeHarness &harness,
                                             const bool validateOnly) {
        return {
            .windowId = harness.core().windowId(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::GuiDocumentCommandContext guiDocumentContext(const RuntimeHarness &harness,
                                                             const bool validateOnly) {
        return {
            .expected = harness.core().documentVersion(),
            .windowId = harness.core().windowId(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::ApplicationCommandContext applicationContext(const bool validateOnly) {
        return {
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    std::optional<EditorObjects> createEditorObjects(RuntimeHarness &harness) {
        Automation::TrackDraftDto track;
        track.clientRef = QStringLiteral("runtime-dimension-track");
        track.name = QStringLiteral("Runtime Dimension Track");
        track.gain = 1.0;
        track.defaultLanguage = QStringLiteral("unknown");

        Automation::ClipDraftDto clip;
        clip.clientRef = QStringLiteral("runtime-dimension-clip");
        clip.type = Automation::ClipDraftDto::Type::Singing;
        clip.properties.name = QStringLiteral("Runtime Dimension Clip");
        clip.properties.start = 0;
        clip.properties.length = 1920;
        clip.properties.clipStart = 0;
        clip.properties.clipLen = 1920;
        clip.properties.gain = 1.0;
        clip.defaultLanguage = QStringLiteral("unknown");
        clip.notes.append({
            .clientRef = QStringLiteral("runtime-dimension-note"),
            .localStart = 0,
            .length = 480,
            .keyIndex = 60,
            .lyric = QStringLiteral("啦"),
            .language = QStringLiteral("cmn"),
        });
        track.clips.append(clip);

        const auto inserted =
            harness.core().project().insertTrack(commandContext(harness), 0, track);
        if (!inserted)
            return std::nullopt;
        EditorObjects result;
        for (const auto &created : inserted.get().createdObjects) {
            if (created.object.kind == Automation::ObjectKind::Track)
                result.trackId = Automation::TrackId(created.object.value);
            else if (created.object.kind == Automation::ObjectKind::Clip)
                result.clipId = Automation::ClipId(created.object.value);
            else if (created.object.kind == Automation::ObjectKind::Note)
                result.noteId = Automation::NoteId(created.object.value);
        }
        if (!result.trackId.isValid() || !result.clipId.isValid() || !result.noteId.isValid())
            return std::nullopt;
        harness.resetHistory();
        return result;
    }

} // namespace RuntimeDimensions
