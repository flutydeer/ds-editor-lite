#include "SettingsAutomationFacade.h"
#include "OperationIds.h"
#include "Modules/FillLyric/Utils/LyricRuleAutomationUtils.h"
#include "Modules/FillLyric/Utils/TaggerRuleOrder.h"

#include <lite/AutomationWire/PublicConstants.h>

#include <QDir>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace Automation {
    namespace {
        constexpr int kMaximumRecentFiles = 10;

        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.message = QStringLiteral("Application settings are unavailable");
            return error;
        }

        AutomationError persistenceError() {
            AutomationError error;
            error.code = AutomationErrorCode::IoError;
            error.message = QStringLiteral("Application settings could not be saved");
            return error;
        }

        bool pathsEqual(const QString &lhs, const QString &rhs) {
#ifdef Q_OS_WIN
            return QString::compare(lhs, rhs, Qt::CaseInsensitive) == 0;
#else
            return lhs == rhs;
#endif
        }

        QStringList normalizedPaths(const QStringList &paths, const int maximum = -1) {
            QStringList result;
            for (const auto &input : paths) {
                const auto path = QDir::cleanPath(input.trimmed());
                if (path.isEmpty() || path == QStringLiteral("."))
                    continue;
                bool duplicate = false;
                for (const auto &existing : std::as_const(result)) {
                    if (pathsEqual(existing, path)) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                    result.append(path);
                if (maximum >= 0 && result.size() >= maximum)
                    break;
            }
            return result;
        }

        AutomationResult<AutomationUnit> validateGeneral(const GeneralSettingsDto &settings) {
            if (settings.uiLanguage != QStringLiteral("system") &&
                settings.uiLanguage != QStringLiteral("en_US") &&
                settings.uiLanguage != QStringLiteral("zh_CN")) {
                return AutomationError::invalidArgument(
                    QStringLiteral("ui_language"), QStringLiteral("UI language is unsupported"));
            }
            if (settings.defaultSingingLanguage.trimmed().isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("default_singing_language"),
                    QStringLiteral("Default singing language is empty"));
            }
            for (auto it = settings.defaultLyrics.cbegin(); it != settings.defaultLyrics.cend();
                 ++it) {
                if (it.key().trimmed().isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("default_lyrics"),
                        QStringLiteral("A default lyric language key is empty"));
                }
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validateAppearance(const AppearanceSettingsDto &settings) {
            if (!std::isfinite(settings.animationTimeScale) || settings.animationTimeScale <= 0.0) {
                return AutomationError::invalidArgument(
                    QStringLiteral("animation_time_scale"),
                    QStringLiteral("Animation time scale must be positive"));
            }
            if (settings.themeId.trimmed().isEmpty()) {
                return AutomationError::invalidArgument(QStringLiteral("theme_id"),
                                                        QStringLiteral("Theme ID is empty"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit>
            validatePublicUiLanguage(const GeneralSettingsDto &settings) {
            if (settings.uiLanguage != QStringLiteral("system") &&
                settings.uiLanguage != QStringLiteral("en_US") &&
                settings.uiLanguage != QStringLiteral("zh_CN")) {
                return AutomationError::invalidArgument(
                    QStringLiteral("ui_language"), QStringLiteral("UI language is unsupported"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validatePublicSinging(const GeneralSettingsDto &settings) {
            if (settings.defaultSingingLanguage.trimmed().isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("default_singing_language"),
                    QStringLiteral("Default singing language is empty"));
            }
            for (auto it = settings.defaultLyrics.cbegin(); it != settings.defaultLyrics.cend();
                 ++it) {
                if (it.key().trimmed().isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("default_lyrics"),
                        QStringLiteral("A default lyric language key is empty"));
                }
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit>
            validatePublicTheme(const AppearanceSettingsDto &settings) {
            if (settings.themeId.trimmed().isEmpty()) {
                return AutomationError::invalidArgument(QStringLiteral("theme_id"),
                                                        QStringLiteral("Theme ID is empty"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validateInference(const InferenceSettingsDto &settings) {
            if (settings.executionProvider != QStringLiteral("CPU") &&
                settings.executionProvider != QStringLiteral("DirectML") &&
                settings.executionProvider != QStringLiteral("CUDA")) {
                return AutomationError::invalidArgument(
                    QStringLiteral("execution_provider"),
                    QStringLiteral("Inference execution provider is unsupported"));
            }
            if (settings.selectedGpuIndex < -1 || settings.samplingSteps < 1 ||
                settings.samplingSteps > 1000 || !std::isfinite(settings.depth) ||
                settings.depth < 0.0 || settings.depth > 1.0 ||
                !std::isfinite(settings.playbackLookaheadSeconds) ||
                settings.playbackLookaheadSeconds <= 0.0 || settings.cacheDirectory.isEmpty() ||
                settings.pitchSmoothKernelSize < 0) {
                return AutomationError::invalidArgument(
                    QStringLiteral("inference"),
                    QStringLiteral("Inference settings contain an invalid value"));
            }
            if (settings.singerSessionCacheCapacity < 0 ||
                settings.singerSessionCacheCapacity > 8 ||
                settings.singerSessionIdleTimeoutSeconds < 0 ||
                settings.singerSessionIdleTimeoutSeconds > 300 ||
                (settings.singerSessionIdleTimeoutSeconds != 0 &&
                 settings.singerSessionIdleTimeoutSeconds % 60 != 0)) {
                return AutomationError::invalidArgument(
                    QStringLiteral("singer_session"),
                    QStringLiteral("Singer session retention settings are invalid"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validateDeveloper(const DeveloperSettingsDto &settings) {
            if (settings.editorRenderBackend != EditorRenderBackend::Legacy &&
                settings.editorRenderBackend != EditorRenderBackend::RhiExperimental) {
                return AutomationError::invalidArgument(
                    QStringLiteral("editor_render_backend"),
                    QStringLiteral("Editor render backend is invalid"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validateG2p(const G2pLanguageSettingsDto &settings) {
            QStringList seen;
            for (const auto &language : settings.languageOrder) {
                const auto normalized = language.trimmed();
                if (normalized.isEmpty() || seen.contains(normalized)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("language_order"),
                        QStringLiteral("G2P language order contains an empty or duplicate entry"));
                }
                seen.append(normalized);
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validateFillLyric(const FillLyricSettingsDto &settings) {
            if (settings.splitMode < 0 || !std::isfinite(settings.textEditFontSize) ||
                settings.textEditFontSize <= 0.0 || !std::isfinite(settings.viewFontSize) ||
                settings.viewFontSize <= 0.0) {
                return AutomationError::invalidArgument(
                    QStringLiteral("fill_lyric"),
                    QStringLiteral("Fill lyric settings contain an invalid value"));
            }
            QStringList splitterNames;
            QSet<QString> ruleIds;
            for (const auto &rule : settings.customSplitterRules) {
                if (rule.name.trimmed().isEmpty() || splitterNames.contains(rule.name)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("custom_splitter_rules"),
                        QStringLiteral("Custom splitter rule names must be non-empty and unique"));
                }
                splitterNames.append(rule.name);
                if (!rule.ruleId.isEmpty() && (!FillLyric::isAutomationRuleId(rule.ruleId) ||
                                               rule.ruleId.startsWith(QStringLiteral("builtin-")) ||
                                               ruleIds.contains(rule.ruleId))) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("custom_splitter_rules.rule_id"),
                        QStringLiteral("Custom lyric rule ID is invalid or duplicated"));
                }
                if (!rule.ruleId.isEmpty())
                    ruleIds.insert(rule.ruleId);
                for (const auto &pattern : rule.regexes) {
                    const auto validation = FillLyric::validateAutomationRegex(pattern);
                    if (!validation.valid) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("custom_splitter_rules.regexes"), validation.error);
                    }
                }
            }
            QStringList taggerNames;
            QStringList taggerLanguages;
            for (const auto &rule : settings.customTaggerRules) {
                if (rule.name.trimmed().isEmpty() || taggerNames.contains(rule.name)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("custom_tagger_rules"),
                        QStringLiteral("Custom tagger rule names must be non-empty and unique"));
                }
                taggerNames.append(rule.name);
                if (rule.language.trimmed().isEmpty() || taggerLanguages.contains(rule.language)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("custom_tagger_rules"),
                        QStringLiteral("Custom tagger languages must be non-empty and unique"));
                }
                taggerLanguages.append(rule.language);
                if (!rule.ruleId.isEmpty() && (!FillLyric::isAutomationRuleId(rule.ruleId) ||
                                               rule.ruleId.startsWith(QStringLiteral("builtin-")) ||
                                               ruleIds.contains(rule.ruleId))) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("custom_tagger_rules.rule_id"),
                        QStringLiteral("Custom lyric rule ID is invalid or duplicated"));
                }
                if (!rule.ruleId.isEmpty())
                    ruleIds.insert(rule.ruleId);
                for (const auto &entry : rule.entries) {
                    if (entry.type != QStringLiteral("regex") &&
                        entry.type != QStringLiteral("array") &&
                        entry.type != QStringLiteral("dict")) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("custom_tagger_rules.entries.type"),
                            QStringLiteral("Custom tagger entry type is unsupported"));
                    }
                    if (entry.type == QStringLiteral("regex")) {
                        for (const auto &pattern : entry.value) {
                            const auto validation = FillLyric::validateAutomationRegex(pattern);
                            if (!validation.valid) {
                                return AutomationError::invalidArgument(
                                    QStringLiteral("custom_tagger_rules.entries.value"),
                                    validation.error);
                            }
                        }
                    }
                }
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validateWindow(const WindowSettingsDto &) {
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validateAudio(const AudioSettingsDto &settings) {
            if (settings.adoptedBufferSize < 0 || !std::isfinite(settings.adoptedSampleRate) ||
                settings.adoptedSampleRate < 0.0 || !std::isfinite(settings.deviceGain) ||
                settings.deviceGain < 0.0 || !std::isfinite(settings.devicePan) ||
                settings.devicePan < -1.0 || settings.devicePan > 1.0 ||
                settings.fileBufferingReadAheadSize < 0 || settings.hotPlugNotificationMode < 0 ||
                settings.hotPlugNotificationMode > 2 || settings.playheadBehavior < 0 ||
                settings.playheadBehavior > 2 || settings.midiDeviceIndex < -1 ||
                !std::isfinite(settings.midiSynthesizerAmplitude) ||
                settings.midiSynthesizerAttackMilliseconds < 0 ||
                settings.midiSynthesizerDecayMilliseconds < 0 ||
                !std::isfinite(settings.midiSynthesizerDecayRatio) ||
                settings.midiSynthesizerDecayRatio < 0.0 ||
                settings.midiSynthesizerDecayRatio > 1.0 ||
                !std::isfinite(settings.midiSynthesizerFrequencyOfA) ||
                settings.midiSynthesizerFrequencyOfA < 0.0 ||
                settings.midiSynthesizerReleaseMilliseconds < 0 || settings.vstEditorPort < 1 ||
                settings.vstEditorPort > 65535 || settings.vstPluginPort < 1 ||
                settings.vstPluginPort > 65535) {
                return AutomationError::invalidArgument(
                    QStringLiteral("audio"),
                    QStringLiteral("Audio settings contain an invalid value"));
            }
            if (settings.pseudoSingerSynthesizers.size() != 4) {
                return AutomationError::invalidArgument(
                    QStringLiteral("pseudo_singer_synthesizers"),
                    QStringLiteral("Exactly four pseudo singer synthesizers are required"));
            }
            for (const auto &synthesizer : settings.pseudoSingerSynthesizers) {
                if (!std::isfinite(synthesizer.amplitude) || synthesizer.attackMilliseconds < 0 ||
                    synthesizer.decayMilliseconds < 0 || !std::isfinite(synthesizer.decayRatio) ||
                    synthesizer.decayRatio < 0.0 || synthesizer.decayRatio > 1.0 ||
                    synthesizer.releaseMilliseconds < 0) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("pseudo_singer_synthesizers"),
                        QStringLiteral("Pseudo singer synthesizer settings are invalid"));
                }
            }
            QStringList presetNames;
            for (const auto &preset : settings.audioExporterPresets) {
                if (preset.name.trimmed().isEmpty() || presetNames.contains(preset.name) ||
                    preset.config.fileType < 0 || preset.config.fileType > 3 ||
                    !std::isfinite(preset.config.sampleRate) || preset.config.sampleRate <= 0.0 ||
                    preset.config.mixingOption < 0 || preset.config.mixingOption > 2 ||
                    preset.config.sourceOption < 0 || preset.config.sourceOption > 2 ||
                    preset.config.timeRange < 0 || preset.config.timeRange > 1) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("audio_exporter_presets"),
                        QStringLiteral("Audio exporter preset is invalid or duplicated"));
                }
                presetNames.append(preset.name);
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit>
            validatePublicAudioDevice(const AudioSettingsDto &settings) {
            if (settings.adoptedBufferSize < 0 || !std::isfinite(settings.adoptedSampleRate) ||
                settings.adoptedSampleRate < 0.0 || settings.hotPlugNotificationMode < 0 ||
                settings.hotPlugNotificationMode > 2 || !std::isfinite(settings.deviceGain) ||
                settings.deviceGain < 0.0 ||
                settings.deviceGain > AutomationWire::MaximumAudioDeviceGain ||
                !std::isfinite(settings.devicePan) || settings.devicePan < -1.0 ||
                settings.devicePan > 1.0) {
                return AutomationError::invalidArgument(
                    QStringLiteral("audio_device"),
                    QStringLiteral("Audio device settings contain an invalid value"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit>
            validatePublicPlaybackBehavior(const AudioSettingsDto &settings) {
            if (settings.playheadBehavior < 0 || settings.playheadBehavior > 2) {
                return AutomationError::invalidArgument(
                    QStringLiteral("behavior"), QStringLiteral("Playback behavior is unsupported"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit>
            validatePublicComputeDevice(const InferenceSettingsDto &settings) {
            if (settings.executionProvider != QStringLiteral("CPU") &&
                settings.executionProvider != QStringLiteral("DirectML") &&
                settings.executionProvider != QStringLiteral("CUDA")) {
                return AutomationError::invalidArgument(
                    QStringLiteral("execution_provider"),
                    QStringLiteral("Inference execution provider is unsupported"));
            }
            if (settings.selectedGpuIndex < -1) {
                return AutomationError::invalidArgument(QStringLiteral("gpu_index"),
                                                        QStringLiteral("GPU index is invalid"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit>
            validatePublicRender(const InferenceSettingsDto &settings) {
            if (settings.samplingSteps < 1 || settings.samplingSteps > 1000 ||
                !std::isfinite(settings.depth) || settings.depth < 0.0 || settings.depth > 1.0 ||
                !std::isfinite(settings.playbackLookaheadSeconds) ||
                settings.playbackLookaheadSeconds <= 0.0 ||
                settings.playbackLookaheadSeconds > 60.0 || settings.pitchSmoothKernelSize < 0 ||
                settings.pitchSmoothKernelSize > 50) {
                return AutomationError::invalidArgument(
                    QStringLiteral("render"),
                    QStringLiteral("Render settings contain an invalid value"));
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit>
            validatePublicSingerSessionRetention(const InferenceSettingsDto &settings) {
            if (settings.singerSessionCacheCapacity < 0 ||
                settings.singerSessionCacheCapacity > 8 ||
                settings.singerSessionIdleTimeoutSeconds < 0 ||
                settings.singerSessionIdleTimeoutSeconds > 300 ||
                (settings.singerSessionIdleTimeoutSeconds != 0 &&
                 settings.singerSessionIdleTimeoutSeconds % 60 != 0)) {
                return AutomationError::invalidArgument(
                    QStringLiteral("singer_session"),
                    QStringLiteral("Singer session retention settings are invalid"));
            }
            return AutomationUnit{};
        }

        SettingsStringCandidateDto stringCandidate(const QString &id, const bool available = true,
                                                   QString unavailableReason = {}) {
            return {
                .id = id,
                .displayName = id,
                .available = available,
                .unavailableReason = std::move(unavailableReason),
            };
        }

        AudioDevicePublicValueDto publicAudioValue(const AudioSettingsDto &audio) {
            return {
                .driverName = audio.driverName,
                .deviceName = audio.deviceName,
                .bufferSize = audio.adoptedBufferSize,
                .sampleRate = audio.adoptedSampleRate,
                .hotPlugNotificationMode = audio.hotPlugNotificationMode,
                .gain = audio.deviceGain,
                .pan = audio.devicePan,
            };
        }

        ComputeDevicePublicValueDto publicComputeValue(const InferenceSettingsDto &inference) {
            return {
                .executionProvider = inference.executionProvider,
                .gpuIndex = inference.selectedGpuIndex,
                .gpuId = inference.selectedGpuId,
            };
        }

        RenderPublicValueDto publicRenderValue(const InferenceSettingsDto &inference) {
            return {
                .samplingSteps = inference.samplingSteps,
                .depth = inference.depth,
                .runVocoderOnCpu = inference.runVocoderOnCpu,
                .autoStartInference = inference.autoStartInference,
                .playbackLookaheadSeconds = inference.playbackLookaheadSeconds,
                .pitchSmoothKernelSize = inference.pitchSmoothKernelSize,
            };
        }

        SingerSessionRetentionPublicValueDto
            publicRetentionValue(const InferenceSettingsDto &inference) {
            return {
                .capacity = inference.singerSessionCacheCapacity,
                .idleTimeoutSeconds = inference.singerSessionIdleTimeoutSeconds,
            };
        }

        PublicSettingsSnapshotDto fallbackPublicSnapshot(const SettingsSnapshotDto &settings) {
            PublicSettingsSnapshotDto result;
            result.uiLanguage = UiLanguagePublicSettingsDto{
                .configured = settings.general.uiLanguage,
                .effective = settings.general.uiLanguage,
                .candidates = {stringCandidate(QStringLiteral("system")),
                               stringCandidate(QStringLiteral("en_US")),
                               stringCandidate(QStringLiteral("zh_CN"))},
            };
            QList<SettingsStringCandidateDto> languageCandidates;
            QStringList languages = settings.general.defaultLyrics.keys();
            if (!languages.contains(settings.general.defaultSingingLanguage))
                languages.append(settings.general.defaultSingingLanguage);
            std::sort(languages.begin(), languages.end());
            for (const auto &language : std::as_const(languages))
                languageCandidates.append(stringCandidate(language));
            result.singing = SingingPublicSettingsDto{
                .configuredDefaultLanguage = settings.general.defaultSingingLanguage,
                .effectiveDefaultLanguage = settings.general.defaultSingingLanguage,
                .configuredDefaultLyrics = settings.general.defaultLyrics,
                .effectiveDefaultLyrics = settings.general.defaultLyrics,
                .languageCandidates = languageCandidates,
            };
            result.theme = ThemePublicSettingsDto{
                .configured = settings.appearance.themeId,
                .effective = settings.appearance.themeId,
                .candidates = {stringCandidate(QStringLiteral("system")),
                               stringCandidate(QStringLiteral("light")),
                               stringCandidate(QStringLiteral("dark"))},
            };
            result.audioDevice = AudioDevicePublicSettingsDto{
                .configured = publicAudioValue(settings.audio),
                .effective = publicAudioValue(settings.audio),
                .unavailableReason = QStringLiteral("Live audio device candidates are unavailable"),
            };
            result.playbackBehavior = PlaybackBehaviorPublicSettingsDto{
                .configured = settings.audio.playheadBehavior,
                .effective = settings.audio.playheadBehavior,
                .candidates = {0, 1, 2},
            };
            QList<SettingsGpuCandidateDto> gpuCandidates;
            if (!settings.inference.selectedGpuId.isEmpty()) {
                gpuCandidates.append({
                    .index = settings.inference.selectedGpuIndex,
                    .id = settings.inference.selectedGpuId,
                    .displayName = settings.inference.selectedGpuId,
                });
            }
            result.computeDevice = ComputeDevicePublicSettingsDto{
                .configured = publicComputeValue(settings.inference),
                .effective = publicComputeValue(settings.inference),
                .providerCandidates = {stringCandidate(QStringLiteral("CPU")),
                                       stringCandidate(QStringLiteral("DirectML")),
                                       stringCandidate(QStringLiteral("CUDA"))},
                .gpuCandidates = gpuCandidates,
                .restartRequiredFields = {},
            };
            result.render = RenderPublicSettingsDto{
                .configured = publicRenderValue(settings.inference),
                .effective = publicRenderValue(settings.inference),
                .samplingStepsRange = {1.0, 1000.0, 1.0},
                .depthRange = {0.0, 1.0, 0.01},
                .playbackLookaheadRange = {1.0, 60.0, 1.0},
                .pitchSmoothKernelRange = {0.0, 50.0, 1.0},
                .restartRequiredFields = {},
            };
            result.singerSessionRetention = SingerSessionRetentionPublicSettingsDto{
                .configured = publicRetentionValue(settings.inference),
                .effective = publicRetentionValue(settings.inference),
                .capacityCandidates = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                .idleTimeoutCandidates = {0, 60, 120, 180, 240, 300},
            };
            result.packageSearchPaths = PackageSearchPathsPublicSettingsDto{
                .configured = settings.general.packageSearchPaths,
                .effective = settings.general.packageSearchPaths,
                .restartRequired = false,
            };
            return result;
        }

        QStringList projectedPaths(const QStringList &paths,
                                   const SettingsPathProjection &projection) {
            if (!projection)
                return paths;
            QStringList result;
            for (const auto &path : paths) {
                if (auto projected = projection(path))
                    result.append(*projected);
            }
            return result;
        }

        bool containsAvailableCandidate(const QList<SettingsStringCandidateDto> &candidates,
                                        const QString &id) {
            return std::any_of(candidates.cbegin(), candidates.cend(), [&](const auto &candidate) {
                return candidate.id == id && candidate.available;
            });
        }

        AutomationResult<AutomationUnit> persisted(const bool success) {
            if (success)
                return AutomationUnit{};
            return persistenceError();
        }

        AutomationError lyricRuleNotFound(const QString &ruleId) {
            AutomationError error;
            error.code = AutomationErrorCode::NotFound;
            error.fieldPath = QStringLiteral("rule_id");
            error.message = QStringLiteral("Lyric rule was not found: %1").arg(ruleId);
            return error;
        }

        AutomationError builtinLyricRuleError() {
            return AutomationError::invalidArgument(
                QStringLiteral("rule_id"),
                QStringLiteral("Built-in lyric rules cannot be updated or deleted"));
        }

        QString lyricRuleOrderKey(const LyricRuleDto &rule) {
            if (!rule.engineOrderKey.isEmpty())
                return rule.engineOrderKey;
            if (rule.kind == LyricRuleKind::Splitter)
                return rule.name;
            return FillLyric::TaggerRuleOrder::key({
                .language = rule.language,
                .builtin = rule.builtin,
            });
        }

        void applyRuleOrder(FillLyricSettingsDto &settings, const LyricRuleKind kind,
                            const QList<LyricRuleDto> &orderedRules) {
            QStringList keys;
            keys.reserve(orderedRules.size());
            for (const auto &rule : orderedRules)
                keys.append(lyricRuleOrderKey(rule));
            if (kind == LyricRuleKind::Splitter) {
                settings.splitterOrder = std::move(keys);
                for (auto &custom : settings.customSplitterRules) {
                    const auto found = std::find_if(
                        orderedRules.cbegin(), orderedRules.cend(), [&custom](const auto &rule) {
                            return !rule.builtin && rule.ruleId == custom.ruleId;
                        });
                    if (found != orderedRules.cend())
                        custom.order =
                            static_cast<int>(std::distance(orderedRules.cbegin(), found));
                }
            } else {
                settings.taggerOrder = std::move(keys);
            }
        }

        QList<LyricRuleDto> rulesOfKind(const QList<LyricRuleDto> &rules,
                                        const LyricRuleKind kind) {
            QList<LyricRuleDto> result;
            for (const auto &rule : rules) {
                if (rule.kind == kind)
                    result.append(rule);
            }
            return result;
        }

        AutomationResult<AutomationUnit> validateRuleDraft(const LyricRuleDraftDto &draft,
                                                           const QList<LyricRuleDto> &rules) {
            if (draft.kind == LyricRuleKind::Splitter) {
                const auto name = draft.name.trimmed();
                if (name.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("name"), QStringLiteral("Splitter rule name is empty"));
                }
                if (std::any_of(rules.cbegin(), rules.cend(), [&](const auto &rule) {
                        return rule.kind == LyricRuleKind::Splitter && rule.name == name;
                    })) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("name"), QStringLiteral("Splitter rule name is duplicated"));
                }
                if (draft.regexes.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("regexes"),
                        QStringLiteral("Splitter rule requires at least one regular expression"));
                }
                for (const auto &pattern : draft.regexes) {
                    const auto validation = FillLyric::validateAutomationRegex(pattern);
                    if (!validation.valid)
                        return AutomationError::invalidArgument(QStringLiteral("regexes"),
                                                                validation.error);
                }
            } else {
                const auto name = draft.name.trimmed();
                if (name.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("name"), QStringLiteral("Tagger rule name is empty"));
                }
                if (std::any_of(rules.cbegin(), rules.cend(), [&](const auto &rule) {
                        return rule.kind == LyricRuleKind::Tagger && !rule.builtin &&
                               rule.name == name;
                    })) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("name"),
                        QStringLiteral("Custom tagger rule name is duplicated"));
                }
                const auto language = draft.language.trimmed();
                if (language.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("language"),
                        QStringLiteral("Tagger rule language is empty"));
                }
                if (std::any_of(rules.cbegin(), rules.cend(), [&](const auto &rule) {
                        return rule.kind == LyricRuleKind::Tagger && !rule.builtin &&
                               rule.language == language;
                    })) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("language"),
                        QStringLiteral("Custom tagger rule language is duplicated"));
                }
                if (draft.entries.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("entries"),
                        QStringLiteral("Tagger rule requires at least one entry"));
                }
            }
            return AutomationUnit{};
        }
    }

    SettingsAutomationFacade::SettingsAutomationFacade(AutomationDispatcher &dispatcher,
                                                       SettingsRuntimeServices services)
        : m_dispatcher(dispatcher), m_services(std::move(services)) {
    }

    AutomationResult<SettingsSnapshotDto> SettingsAutomationFacade::getSettings() {
        return m_dispatcher.dispatchApplicationQuery<SettingsSnapshotDto>(
            OperationIds::settings::query, [this] {
                if (!m_services.snapshot)
                    return AutomationResult<SettingsSnapshotDto>(unavailable());
                return AutomationResult<SettingsSnapshotDto>(m_services.snapshot());
            });
    }

    QStringList SettingsAutomationFacade::publicSettingsDomains() {
        return {
            QStringLiteral("ui_language"),
            QStringLiteral("singing"),
            QStringLiteral("theme"),
            QStringLiteral("audio_device"),
            QStringLiteral("playback_behavior"),
            QStringLiteral("compute_device"),
            QStringLiteral("render"),
            QStringLiteral("singer_session_retention"),
            QStringLiteral("package_search_paths"),
        };
    }

    AutomationResult<PublicSettingsSnapshotDto>
        SettingsAutomationFacade::queryPublicSettings(QStringList domains,
                                                      SettingsPathProjection pathProjection) {
        return m_dispatcher.dispatchApplicationQuery<PublicSettingsSnapshotDto>(
            OperationIds::settings::query,
            [this, domains = std::move(domains), pathProjection = std::move(pathProjection)] {
                if (!m_services.snapshot)
                    return AutomationResult<PublicSettingsSnapshotDto>(unavailable());
                const auto allowed = publicSettingsDomains();
                QSet<QString> requested;
                for (auto domain : domains) {
                    domain = domain.trimmed();
                    if (!allowed.contains(domain)) {
                        return AutomationResult<PublicSettingsSnapshotDto>(
                            AutomationError::invalidArgument(
                                QStringLiteral("domains"),
                                QStringLiteral("Unknown public settings domain: %1").arg(domain)));
                    }
                    requested.insert(domain);
                }
                auto result = m_services.publicSnapshot
                                  ? m_services.publicSnapshot()
                                  : fallbackPublicSnapshot(m_services.snapshot());
                const auto selected = [&requested, &domains](const QString &domain) {
                    return domains.isEmpty() || requested.contains(domain);
                };
                if (!selected(QStringLiteral("ui_language")))
                    result.uiLanguage.reset();
                if (!selected(QStringLiteral("singing")))
                    result.singing.reset();
                if (!selected(QStringLiteral("theme")))
                    result.theme.reset();
                if (!selected(QStringLiteral("audio_device")))
                    result.audioDevice.reset();
                if (!selected(QStringLiteral("playback_behavior")))
                    result.playbackBehavior.reset();
                if (!selected(QStringLiteral("compute_device")))
                    result.computeDevice.reset();
                if (!selected(QStringLiteral("render")))
                    result.render.reset();
                if (!selected(QStringLiteral("singer_session_retention")))
                    result.singerSessionRetention.reset();
                if (!selected(QStringLiteral("package_search_paths"))) {
                    result.packageSearchPaths.reset();
                } else if (result.packageSearchPaths) {
                    result.packageSearchPaths->configured =
                        projectedPaths(result.packageSearchPaths->configured, pathProjection);
                    result.packageSearchPaths->effective =
                        projectedPaths(result.packageSearchPaths->effective, pathProjection);
                }
                return AutomationResult<PublicSettingsSnapshotDto>(std::move(result));
            });
    }

    AutomationResult<SettingsMutationResultDto> SettingsAutomationFacade::updatePublicSettings(
        const OperationId &operationId, const ApplicationCommandContext &context,
        PublicSettingsMutation mutation, PublicSettingsValidator validator,
        PublicSettingsApply apply, RestartFieldResolver restartFields) {
        return m_dispatcher.dispatchApplicationCommand<SettingsMutationResultDto>(
            operationId, context,
            [this, mutation = std::move(mutation), validator = std::move(validator),
             apply = std::move(apply),
             restartFields = std::move(restartFields)](const bool validateOnly) {
                if (!m_services.snapshot || !apply)
                    return AutomationResult<SettingsMutationResultDto>(unavailable());
                const auto current = m_services.snapshot();
                auto target = current;
                if (mutation) {
                    auto mutated = mutation(target);
                    if (!mutated)
                        return AutomationResult<SettingsMutationResultDto>(mutated.getError());
                }
                if (validator) {
                    auto validation = validator(target);
                    if (!validation)
                        return AutomationResult<SettingsMutationResultDto>(validation.getError());
                }
                const auto changed = current != target;
                auto restart =
                    restartFields && changed ? restartFields(current, target) : QStringList{};
                if (!validateOnly && changed) {
                    auto applied = apply(target);
                    if (!applied)
                        return AutomationResult<SettingsMutationResultDto>(applied.getError());
                }
                return AutomationResult<SettingsMutationResultDto>({
                    .changed = changed,
                    .validatedOnly = validateOnly,
                    .restartRequired = !restart.isEmpty(),
                    .restartRequiredFields = std::move(restart),
                });
            });
    }

    AutomationResult<AutomationUnit> SettingsAutomationFacade::validateFillLyricTarget(
        const FillLyricSettingsDto &target) const {
        auto validation = validateFillLyric(target);
        if (!validation)
            return validation;
        if (m_services.validateFillLyricRuntime)
            return m_services.validateFillLyricRuntime(target);
        return AutomationUnit{};
    }

    AutomationResult<SettingsMutationResultDto>
        SettingsAutomationFacade::updateUiLanguage(const ApplicationCommandContext &context,
                                                   const UiLanguageSettingsPatchDto &patch) {
        return updatePublicSettings(
            OperationIds::settings::update_general, context,
            [patch](SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                if (patch.uiLanguage)
                    target.general.uiLanguage = patch.uiLanguage->trimmed();
                return AutomationUnit{};
            },
            [this](const SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                const auto validation = validatePublicUiLanguage(target.general);
                if (!validation)
                    return validation;
                const auto publicSnapshot = m_services.publicSnapshot
                                                ? m_services.publicSnapshot()
                                                : fallbackPublicSnapshot(target);
                if (publicSnapshot.uiLanguage &&
                    !containsAvailableCandidate(publicSnapshot.uiLanguage->candidates,
                                                target.general.uiLanguage)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("ui_language"),
                        QStringLiteral("UI language is unavailable"));
                }
                return AutomationUnit{};
            },
            [this](const SettingsSnapshotDto &target) {
                if (m_services.applyUiLanguage)
                    return m_services.applyUiLanguage(target.general);
                if (!m_services.applyGeneral)
                    return AutomationResult<AutomationUnit>(unavailable());
                return persisted(m_services.applyGeneral(target.general));
            });
    }

    AutomationResult<SettingsMutationResultDto>
        SettingsAutomationFacade::updateSinging(const ApplicationCommandContext &context,
                                                const SingingSettingsPatchDto &patch) {
        return updatePublicSettings(
            OperationIds::settings::update_general, context,
            [patch](SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                if (patch.defaultLanguage)
                    target.general.defaultSingingLanguage = patch.defaultLanguage->trimmed();
                if (patch.defaultLyrics)
                    target.general.defaultLyrics = *patch.defaultLyrics;
                return AutomationUnit{};
            },
            [](const SettingsSnapshotDto &target) { return validatePublicSinging(target.general); },
            [this](const SettingsSnapshotDto &target) {
                if (!m_services.applyGeneral)
                    return AutomationResult<AutomationUnit>(unavailable());
                return persisted(m_services.applyGeneral(target.general));
            });
    }

    AutomationResult<SettingsMutationResultDto>
        SettingsAutomationFacade::updateTheme(const ApplicationCommandContext &context,
                                              const ThemeSettingsPatchDto &patch) {
        return updatePublicSettings(
            OperationIds::settings::update_appearance, context,
            [patch](SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                if (patch.themeId)
                    target.appearance.themeId = patch.themeId->trimmed();
                return AutomationUnit{};
            },
            [this](const SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                const auto validation = validatePublicTheme(target.appearance);
                if (!validation)
                    return validation;
                const auto publicSnapshot = m_services.publicSnapshot
                                                ? m_services.publicSnapshot()
                                                : fallbackPublicSnapshot(target);
                if (publicSnapshot.theme &&
                    !containsAvailableCandidate(publicSnapshot.theme->candidates,
                                                target.appearance.themeId)) {
                    return AutomationError::invalidArgument(QStringLiteral("theme_id"),
                                                            QStringLiteral("Theme is unavailable"));
                }
                return AutomationUnit{};
            },
            [this](const SettingsSnapshotDto &target) {
                if (m_services.applyTheme)
                    return m_services.applyTheme(target.appearance);
                if (!m_services.applyAppearance)
                    return AutomationResult<AutomationUnit>(unavailable());
                return persisted(m_services.applyAppearance(target.appearance));
            });
    }

    AutomationResult<SettingsMutationResultDto>
        SettingsAutomationFacade::updateAudioDevice(const ApplicationCommandContext &context,
                                                    const AudioDeviceSettingsPatchDto &patch) {
        return updatePublicSettings(
            OperationIds::settings::update_audio, context,
            [patch](SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                if (patch.driverName)
                    target.audio.driverName = patch.driverName->trimmed();
                if (patch.deviceName)
                    target.audio.deviceName = patch.deviceName->trimmed();
                if (patch.bufferSize)
                    target.audio.adoptedBufferSize = *patch.bufferSize;
                if (patch.sampleRate)
                    target.audio.adoptedSampleRate = *patch.sampleRate;
                if (patch.hotPlugNotificationMode)
                    target.audio.hotPlugNotificationMode = *patch.hotPlugNotificationMode;
                if (patch.gain)
                    target.audio.deviceGain = *patch.gain;
                if (patch.pan)
                    target.audio.devicePan = *patch.pan;
                return AutomationUnit{};
            },
            [this, patch](const SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                auto validation = validatePublicAudioDevice(target.audio);
                if (!validation)
                    return validation;
                const bool requiresDeviceCandidate =
                    patch.driverName || patch.deviceName || patch.bufferSize || patch.sampleRate;
                if (!requiresDeviceCandidate)
                    return AutomationUnit{};
                const auto publicSnapshot = m_services.publicSnapshot
                                                ? m_services.publicSnapshot()
                                                : fallbackPublicSnapshot(target);
                if (!publicSnapshot.audioDevice || publicSnapshot.audioDevice->drivers.isEmpty())
                    return AutomationUnit{};
                const auto &drivers = publicSnapshot.audioDevice->drivers;
                const auto driver =
                    std::find_if(drivers.cbegin(), drivers.cend(), [&](const auto &d) {
                        return d.id == target.audio.driverName && d.available;
                    });
                if (driver == drivers.cend()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("driver_name"),
                        QStringLiteral("Audio driver is unavailable"));
                }
                if (driver->devices.isEmpty() || (patch.driverName && !patch.deviceName))
                    return AutomationUnit{};
                const auto device = std::find_if(
                    driver->devices.cbegin(), driver->devices.cend(),
                    [&](const auto &d) { return d.id == target.audio.deviceName && d.available; });
                if (device == driver->devices.cend()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("device_name"),
                        QStringLiteral("Audio device is unavailable"));
                }
                if (patch.bufferSize && !device->bufferSizes.isEmpty() &&
                    !device->bufferSizes.contains(target.audio.adoptedBufferSize)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("buffer_size"),
                        QStringLiteral("Audio buffer size is unavailable"));
                }
                if (patch.sampleRate && !device->sampleRates.isEmpty() &&
                    !device->sampleRates.contains(target.audio.adoptedSampleRate)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("sample_rate"),
                        QStringLiteral("Audio sample rate is unavailable"));
                }
                return AutomationUnit{};
            },
            [this, patch](const SettingsSnapshotDto &target) {
                if (m_services.applyAudioDevice)
                    return m_services.applyAudioDevice(target.audio, patch);
                if (!m_services.applyAudio)
                    return AutomationResult<AutomationUnit>(unavailable());
                return persisted(m_services.applyAudio(target.audio));
            });
    }

    AutomationResult<SettingsMutationResultDto> SettingsAutomationFacade::updatePlaybackBehavior(
        const ApplicationCommandContext &context, const PlaybackBehaviorSettingsPatchDto &patch) {
        return updatePublicSettings(
            OperationIds::settings::update_audio, context,
            [patch](SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                if (patch.behavior)
                    target.audio.playheadBehavior = *patch.behavior;
                return AutomationUnit{};
            },
            [](const SettingsSnapshotDto &target) {
                return validatePublicPlaybackBehavior(target.audio);
            },
            [this](const SettingsSnapshotDto &target) {
                if (!m_services.applyAudio)
                    return AutomationResult<AutomationUnit>(unavailable());
                return persisted(m_services.applyAudio(target.audio));
            });
    }

    AutomationResult<SettingsMutationResultDto>
        SettingsAutomationFacade::updateComputeDevice(const ApplicationCommandContext &context,
                                                      const ComputeDeviceSettingsPatchDto &patch) {
        return updatePublicSettings(
            OperationIds::settings::update_inference, context,
            [patch](SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                if (patch.executionProvider)
                    target.inference.executionProvider = patch.executionProvider->trimmed();
                if (patch.gpuIndex)
                    target.inference.selectedGpuIndex = *patch.gpuIndex;
                if (patch.gpuId)
                    target.inference.selectedGpuId = patch.gpuId->trimmed();
                return AutomationUnit{};
            },
            [this](const SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                auto validation = validatePublicComputeDevice(target.inference);
                if (!validation)
                    return validation;
                const auto publicSnapshot = m_services.publicSnapshot
                                                ? m_services.publicSnapshot()
                                                : fallbackPublicSnapshot(target);
                if (publicSnapshot.computeDevice &&
                    !containsAvailableCandidate(publicSnapshot.computeDevice->providerCandidates,
                                                target.inference.executionProvider)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("execution_provider"),
                        QStringLiteral("Inference provider is unavailable"));
                }
                if (target.inference.executionProvider != QStringLiteral("CPU") &&
                    publicSnapshot.computeDevice &&
                    !publicSnapshot.computeDevice->gpuCandidates.isEmpty() &&
                    !target.inference.selectedGpuId.isEmpty()) {
                    const auto &candidates = publicSnapshot.computeDevice->gpuCandidates;
                    const auto found = std::any_of(
                        candidates.cbegin(), candidates.cend(), [&](const auto &candidate) {
                            return candidate.available &&
                                   candidate.id == target.inference.selectedGpuId &&
                                   (target.inference.selectedGpuIndex < 0 ||
                                    candidate.index == target.inference.selectedGpuIndex);
                        });
                    if (!found) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("gpu_id"),
                            QStringLiteral("Inference GPU is unavailable"));
                    }
                }
                return AutomationUnit{};
            },
            [this](const SettingsSnapshotDto &target) {
                if (!m_services.applyInference)
                    return AutomationResult<AutomationUnit>(unavailable());
                return persisted(m_services.applyInference(target.inference));
            },
            [](const SettingsSnapshotDto &before, const SettingsSnapshotDto &after) {
                QStringList fields;
                if (before.inference.executionProvider != after.inference.executionProvider)
                    fields.append(QStringLiteral("execution_provider"));
                if (before.inference.selectedGpuIndex != after.inference.selectedGpuIndex)
                    fields.append(QStringLiteral("gpu_index"));
                if (before.inference.selectedGpuId != after.inference.selectedGpuId)
                    fields.append(QStringLiteral("gpu_id"));
                return fields;
            });
    }

    AutomationResult<SettingsMutationResultDto>
        SettingsAutomationFacade::updateRender(const ApplicationCommandContext &context,
                                               const RenderSettingsPatchDto &patch) {
        return updatePublicSettings(
            OperationIds::settings::update_inference, context,
            [patch](SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                if (patch.samplingSteps)
                    target.inference.samplingSteps = *patch.samplingSteps;
                if (patch.depth)
                    target.inference.depth = *patch.depth;
                if (patch.runVocoderOnCpu)
                    target.inference.runVocoderOnCpu = *patch.runVocoderOnCpu;
                if (patch.autoStartInference)
                    target.inference.autoStartInference = *patch.autoStartInference;
                if (patch.playbackLookaheadSeconds)
                    target.inference.playbackLookaheadSeconds = *patch.playbackLookaheadSeconds;
                if (patch.pitchSmoothKernelSize)
                    target.inference.pitchSmoothKernelSize = *patch.pitchSmoothKernelSize;
                return AutomationUnit{};
            },
            [](const SettingsSnapshotDto &target) {
                return validatePublicRender(target.inference);
            },
            [this](const SettingsSnapshotDto &target) {
                if (!m_services.applyInference)
                    return AutomationResult<AutomationUnit>(unavailable());
                return persisted(m_services.applyInference(target.inference));
            },
            [](const SettingsSnapshotDto &before, const SettingsSnapshotDto &after) {
                return before.inference.runVocoderOnCpu != after.inference.runVocoderOnCpu
                           ? QStringList{QStringLiteral("run_vocoder_on_cpu")}
                           : QStringList{};
            });
    }

    AutomationResult<SettingsMutationResultDto>
        SettingsAutomationFacade::updateSingerSessionRetention(
            const ApplicationCommandContext &context,
            const SingerSessionRetentionSettingsPatchDto &patch) {
        return updatePublicSettings(
            OperationIds::settings::update_inference, context,
            [patch](SettingsSnapshotDto &target) -> AutomationResult<AutomationUnit> {
                if (patch.capacity)
                    target.inference.singerSessionCacheCapacity = *patch.capacity;
                if (patch.idleTimeoutSeconds)
                    target.inference.singerSessionIdleTimeoutSeconds = *patch.idleTimeoutSeconds;
                return AutomationUnit{};
            },
            [](const SettingsSnapshotDto &target) {
                return validatePublicSingerSessionRetention(target.inference);
            },
            [this](const SettingsSnapshotDto &target) {
                if (!m_services.applyInference)
                    return AutomationResult<AutomationUnit>(unavailable());
                return persisted(m_services.applyInference(target.inference));
            });
    }

    AutomationResult<QList<LyricRuleDto>> SettingsAutomationFacade::listLyricRules() {
        return m_dispatcher.dispatchApplicationQuery<QList<LyricRuleDto>>(
            OperationIds::settings::query, [this] {
                if (!m_services.lyricRules)
                    return AutomationResult<QList<LyricRuleDto>>(unavailable());
                return AutomationResult<QList<LyricRuleDto>>(m_services.lyricRules());
            });
    }

    AutomationResult<LyricRuleMutationResultDto>
        SettingsAutomationFacade::createLyricRule(const ApplicationCommandContext &context,
                                                  const LyricRuleDraftDto &draft) {
        const auto newRuleId = FillLyric::createAutomationRuleId();
        return m_dispatcher.dispatchApplicationCommand<LyricRuleMutationResultDto>(
            OperationIds::settings::update_fill_lyric, context,
            [this, draft, newRuleId](const bool validateOnly) {
                if (!m_services.snapshot || !m_services.applyFillLyric || !m_services.lyricRules)
                    return AutomationResult<LyricRuleMutationResultDto>(unavailable());

                const auto currentRules = m_services.lyricRules();
                const auto draftValidation = validateRuleDraft(draft, currentRules);
                if (!draftValidation)
                    return AutomationResult<LyricRuleMutationResultDto>(draftValidation.getError());

                auto ordered = rulesOfKind(currentRules, draft.kind);
                const auto position = draft.position.value_or(ordered.size());
                if (position < 0 || position > ordered.size()) {
                    return AutomationResult<LyricRuleMutationResultDto>(
                        AutomationError::invalidArgument(
                            QStringLiteral("position"),
                            QStringLiteral("Lyric rule position is outside the valid range")));
                }

                LyricRuleDto created{
                    .ruleId = newRuleId,
                    .kind = draft.kind,
                    .builtin = false,
                    .name = draft.name.trimmed(),
                    .language = draft.language.trimmed(),
                    .regexes = draft.regexes,
                    .entries = draft.entries,
                    .enabled = draft.enabled,
                    .order = position,
                };
                created.engineOrderKey = lyricRuleOrderKey(created);

                auto target = m_services.snapshot().fillLyric;
                if (draft.kind == LyricRuleKind::Splitter) {
                    target.customSplitterRules.append({
                        .ruleId = newRuleId,
                        .name = created.name,
                        .regexes = created.regexes,
                        .enabled = created.enabled,
                        .order = position,
                    });
                } else {
                    target.customTaggerRules.append({
                        .ruleId = newRuleId,
                        .name = created.name,
                        .language = created.language,
                        .entries = created.entries,
                        .enabled = created.enabled,
                    });
                }
                ordered.insert(position, created);
                for (qsizetype index = 0; index < ordered.size(); ++index)
                    ordered[index].order = static_cast<int>(index);
                applyRuleOrder(target, draft.kind, ordered);

                const auto validation = validateFillLyricTarget(target);
                if (!validation)
                    return AutomationResult<LyricRuleMutationResultDto>(validation.getError());
                if (!validateOnly && !m_services.applyFillLyric(target))
                    return AutomationResult<LyricRuleMutationResultDto>(persistenceError());
                return AutomationResult<LyricRuleMutationResultDto>({
                    .changed = true,
                    .validatedOnly = validateOnly,
                    .rule = std::move(created),
                });
            });
    }

    AutomationResult<LyricRuleMutationResultDto>
        SettingsAutomationFacade::updateLyricRule(const ApplicationCommandContext &context,
                                                  const QString &ruleId,
                                                  const LyricRulePatchDto &patch) {
        return m_dispatcher.dispatchApplicationCommand<LyricRuleMutationResultDto>(
            OperationIds::settings::update_fill_lyric, context,
            [this, ruleId = ruleId.trimmed(), patch](const bool validateOnly) {
                if (!m_services.snapshot || !m_services.applyFillLyric || !m_services.lyricRules)
                    return AutomationResult<LyricRuleMutationResultDto>(unavailable());
                const auto rules = m_services.lyricRules();
                const auto found =
                    std::find_if(rules.cbegin(), rules.cend(),
                                 [&](const auto &rule) { return rule.ruleId == ruleId; });
                if (found == rules.cend())
                    return AutomationResult<LyricRuleMutationResultDto>(lyricRuleNotFound(ruleId));
                if (found->builtin)
                    return AutomationResult<LyricRuleMutationResultDto>(builtinLyricRuleError());

                if (found->kind == LyricRuleKind::Splitter && (patch.language || patch.entries)) {
                    return AutomationResult<LyricRuleMutationResultDto>(
                        AutomationError::invalidArgument(
                            patch.language ? QStringLiteral("language") : QStringLiteral("entries"),
                            QStringLiteral("Field is not applicable to splitter rules")));
                }
                if (found->kind == LyricRuleKind::Tagger && patch.regexes) {
                    return AutomationResult<LyricRuleMutationResultDto>(
                        AutomationError::invalidArgument(
                            QStringLiteral("regexes"),
                            QStringLiteral("Field is not applicable to tagger rules")));
                }

                auto updated = *found;
                if (patch.name)
                    updated.name = patch.name->trimmed();
                if (patch.language)
                    updated.language = patch.language->trimmed();
                if (patch.regexes)
                    updated.regexes = *patch.regexes;
                if (patch.entries)
                    updated.entries = *patch.entries;

                LyricRuleDraftDto candidate{
                    .kind = updated.kind,
                    .name = updated.name,
                    .language = updated.language,
                    .regexes = updated.regexes,
                    .entries = updated.entries,
                    .enabled = updated.enabled,
                };
                auto otherRules = rules;
                otherRules.erase(
                    std::remove_if(otherRules.begin(), otherRules.end(),
                                   [&](const auto &rule) { return rule.ruleId == ruleId; }),
                    otherRules.end());
                const auto candidateValidation = validateRuleDraft(candidate, otherRules);
                if (!candidateValidation) {
                    return AutomationResult<LyricRuleMutationResultDto>(
                        candidateValidation.getError());
                }

                auto target = m_services.snapshot().fillLyric;
                if (updated.kind == LyricRuleKind::Splitter) {
                    const auto custom = std::find_if(
                        target.customSplitterRules.begin(), target.customSplitterRules.end(),
                        [&](const auto &rule) { return rule.ruleId == ruleId; });
                    if (custom == target.customSplitterRules.end())
                        return AutomationResult<LyricRuleMutationResultDto>(
                            lyricRuleNotFound(ruleId));
                    custom->name = updated.name;
                    custom->regexes = updated.regexes;
                } else {
                    const auto custom = std::find_if(
                        target.customTaggerRules.begin(), target.customTaggerRules.end(),
                        [&](const auto &rule) { return rule.ruleId == ruleId; });
                    if (custom == target.customTaggerRules.end())
                        return AutomationResult<LyricRuleMutationResultDto>(
                            lyricRuleNotFound(ruleId));
                    custom->name = updated.name;
                    custom->language = updated.language;
                    custom->entries = updated.entries;
                }

                auto ordered = rulesOfKind(rules, updated.kind);
                const auto orderedRule =
                    std::find_if(ordered.begin(), ordered.end(),
                                 [&](const auto &rule) { return rule.ruleId == ruleId; });
                if (orderedRule == ordered.end())
                    return AutomationResult<LyricRuleMutationResultDto>(lyricRuleNotFound(ruleId));
                *orderedRule = updated;
                orderedRule->engineOrderKey = lyricRuleOrderKey(updated);
                if (updated.kind == LyricRuleKind::Splitter)
                    orderedRule->engineOrderKey = updated.name;
                else
                    orderedRule->engineOrderKey = FillLyric::TaggerRuleOrder::key(
                        {.language = updated.language, .builtin = false});
                applyRuleOrder(target, updated.kind, ordered);

                const auto validation = validateFillLyricTarget(target);
                if (!validation)
                    return AutomationResult<LyricRuleMutationResultDto>(validation.getError());
                const bool changed = updated != *found;
                if (!validateOnly && changed && !m_services.applyFillLyric(target))
                    return AutomationResult<LyricRuleMutationResultDto>(persistenceError());
                return AutomationResult<LyricRuleMutationResultDto>({
                    .changed = changed,
                    .validatedOnly = validateOnly,
                    .rule = std::move(updated),
                });
            });
    }

    AutomationResult<LyricRuleDeleteResultDto>
        SettingsAutomationFacade::deleteLyricRule(const ApplicationCommandContext &context,
                                                  const QString &ruleId) {
        return m_dispatcher.dispatchApplicationCommand<LyricRuleDeleteResultDto>(
            OperationIds::settings::update_fill_lyric, context,
            [this, ruleId = ruleId.trimmed()](const bool validateOnly) {
                if (!m_services.snapshot || !m_services.applyFillLyric || !m_services.lyricRules)
                    return AutomationResult<LyricRuleDeleteResultDto>(unavailable());
                const auto rules = m_services.lyricRules();
                const auto found =
                    std::find_if(rules.cbegin(), rules.cend(),
                                 [&](const auto &rule) { return rule.ruleId == ruleId; });
                if (found == rules.cend())
                    return AutomationResult<LyricRuleDeleteResultDto>(lyricRuleNotFound(ruleId));
                if (found->builtin)
                    return AutomationResult<LyricRuleDeleteResultDto>(builtinLyricRuleError());

                auto target = m_services.snapshot().fillLyric;
                if (found->kind == LyricRuleKind::Splitter) {
                    target.customSplitterRules.removeIf(
                        [&](const auto &rule) { return rule.ruleId == ruleId; });
                } else {
                    target.customTaggerRules.removeIf(
                        [&](const auto &rule) { return rule.ruleId == ruleId; });
                }
                auto ordered = rulesOfKind(rules, found->kind);
                ordered.removeIf([&](const auto &rule) { return rule.ruleId == ruleId; });
                applyRuleOrder(target, found->kind, ordered);
                const auto validation = validateFillLyricTarget(target);
                if (!validation)
                    return AutomationResult<LyricRuleDeleteResultDto>(validation.getError());
                if (!validateOnly && !m_services.applyFillLyric(target))
                    return AutomationResult<LyricRuleDeleteResultDto>(persistenceError());
                return AutomationResult<LyricRuleDeleteResultDto>({
                    .changed = true,
                    .validatedOnly = validateOnly,
                    .ruleId = ruleId,
                });
            });
    }

    AutomationResult<LyricRuleMutationResultDto>
        SettingsAutomationFacade::setLyricRuleEnabled(const ApplicationCommandContext &context,
                                                      const QString &ruleId, const bool enabled) {
        return m_dispatcher.dispatchApplicationCommand<LyricRuleMutationResultDto>(
            OperationIds::settings::update_fill_lyric, context,
            [this, ruleId = ruleId.trimmed(), enabled](const bool validateOnly) {
                if (!m_services.snapshot || !m_services.applyFillLyric || !m_services.lyricRules)
                    return AutomationResult<LyricRuleMutationResultDto>(unavailable());
                const auto rules = m_services.lyricRules();
                const auto found =
                    std::find_if(rules.cbegin(), rules.cend(),
                                 [&](const auto &rule) { return rule.ruleId == ruleId; });
                if (found == rules.cend())
                    return AutomationResult<LyricRuleMutationResultDto>(lyricRuleNotFound(ruleId));

                auto target = m_services.snapshot().fillLyric;
                if (found->builtin) {
                    if (found->kind == LyricRuleKind::Splitter)
                        target.builtinSplitterEnabled.insert(found->name, enabled);
                    else
                        target.builtinTaggerEnabled.insert(found->language, enabled);
                } else if (found->kind == LyricRuleKind::Splitter) {
                    const auto custom = std::find_if(
                        target.customSplitterRules.begin(), target.customSplitterRules.end(),
                        [&](const auto &rule) { return rule.ruleId == ruleId; });
                    if (custom == target.customSplitterRules.end())
                        return AutomationResult<LyricRuleMutationResultDto>(
                            lyricRuleNotFound(ruleId));
                    custom->enabled = enabled;
                } else {
                    const auto custom = std::find_if(
                        target.customTaggerRules.begin(), target.customTaggerRules.end(),
                        [&](const auto &rule) { return rule.ruleId == ruleId; });
                    if (custom == target.customTaggerRules.end())
                        return AutomationResult<LyricRuleMutationResultDto>(
                            lyricRuleNotFound(ruleId));
                    custom->enabled = enabled;
                }
                const auto validation = validateFillLyricTarget(target);
                if (!validation)
                    return AutomationResult<LyricRuleMutationResultDto>(validation.getError());
                const bool changed = found->enabled != enabled;
                if (!validateOnly && changed && !m_services.applyFillLyric(target))
                    return AutomationResult<LyricRuleMutationResultDto>(persistenceError());
                auto updated = *found;
                updated.enabled = enabled;
                return AutomationResult<LyricRuleMutationResultDto>({
                    .changed = changed,
                    .validatedOnly = validateOnly,
                    .rule = std::move(updated),
                });
            });
    }

    AutomationResult<LyricRuleMutationResultDto>
        SettingsAutomationFacade::moveLyricRule(const ApplicationCommandContext &context,
                                                const QString &ruleId, const int targetIndex) {
        return m_dispatcher.dispatchApplicationCommand<LyricRuleMutationResultDto>(
            OperationIds::settings::update_fill_lyric, context,
            [this, ruleId = ruleId.trimmed(), targetIndex](const bool validateOnly) {
                if (!m_services.snapshot || !m_services.applyFillLyric || !m_services.lyricRules)
                    return AutomationResult<LyricRuleMutationResultDto>(unavailable());
                const auto rules = m_services.lyricRules();
                const auto found =
                    std::find_if(rules.cbegin(), rules.cend(),
                                 [&](const auto &rule) { return rule.ruleId == ruleId; });
                if (found == rules.cend())
                    return AutomationResult<LyricRuleMutationResultDto>(lyricRuleNotFound(ruleId));
                auto ordered = rulesOfKind(rules, found->kind);
                if (targetIndex < 0 || targetIndex >= ordered.size()) {
                    return AutomationResult<LyricRuleMutationResultDto>(
                        AutomationError::invalidArgument(
                            QStringLiteral("target_index"),
                            QStringLiteral("Lyric rule target index is outside the valid range")));
                }
                const auto current =
                    std::find_if(ordered.cbegin(), ordered.cend(),
                                 [&](const auto &rule) { return rule.ruleId == ruleId; });
                if (current == ordered.cend())
                    return AutomationResult<LyricRuleMutationResultDto>(lyricRuleNotFound(ruleId));
                const auto currentIndex =
                    static_cast<int>(std::distance(ordered.cbegin(), current));
                auto moved = *current;
                ordered.removeAt(currentIndex);
                ordered.insert(targetIndex, moved);
                for (qsizetype index = 0; index < ordered.size(); ++index)
                    ordered[index].order = static_cast<int>(index);
                moved.order = targetIndex;

                auto target = m_services.snapshot().fillLyric;
                applyRuleOrder(target, found->kind, ordered);
                const auto validation = validateFillLyricTarget(target);
                if (!validation)
                    return AutomationResult<LyricRuleMutationResultDto>(validation.getError());
                const bool changed = currentIndex != targetIndex;
                if (!validateOnly && changed && !m_services.applyFillLyric(target))
                    return AutomationResult<LyricRuleMutationResultDto>(persistenceError());
                return AutomationResult<LyricRuleMutationResultDto>({
                    .changed = changed,
                    .validatedOnly = validateOnly,
                    .rule = std::move(moved),
                });
            });
    }

    AutomationResult<LyricRuleTestResultDto>
        SettingsAutomationFacade::testLyricRules(const QString &text) {
        return m_dispatcher.dispatchApplicationQuery<LyricRuleTestResultDto>(
            OperationIds::settings::query, [this, text] {
                if (!m_services.testLyricRules)
                    return AutomationResult<LyricRuleTestResultDto>(unavailable());
                return m_services.testLyricRules(text);
            });
    }

    template <typename T, typename Getter, typename Validator, typename Apply>
    AutomationResult<ApplicationMutationResult> SettingsAutomationFacade::update(
        const OperationId &operationId, const ApplicationCommandContext &context, const T &settings,
        Getter getter, Validator validator, Apply apply) {
        return m_dispatcher.dispatchApplicationCommand<ApplicationMutationResult>(
            operationId, context,
            [this, &settings, getter, validator, apply](const bool validateOnly) {
                if (!m_services.snapshot || !apply)
                    return AutomationResult<ApplicationMutationResult>(unavailable());
                const auto validation = validator(settings);
                if (!validation)
                    return AutomationResult<ApplicationMutationResult>(validation.getError());
                const auto current = getter(m_services.snapshot());
                const bool changed = current != settings;
                if (!validateOnly && changed && !apply(settings))
                    return AutomationResult<ApplicationMutationResult>(persistenceError());
                return AutomationResult<ApplicationMutationResult>({
                    .changed = changed,
                    .validatedOnly = validateOnly,
                });
            });
    }

    AutomationResult<ApplicationMutationResult> SettingsAutomationFacade::updateGeneralState(
        const OperationId &operationId, const ApplicationCommandContext &context,
        GeneralMutation mutation, std::optional<AutomationError> validationError) {
        return m_dispatcher.dispatchApplicationCommand<ApplicationMutationResult>(
            operationId, context,
            [this, mutation = std::move(mutation),
             validationError = std::move(validationError)](const bool validateOnly) {
                if (validationError)
                    return AutomationResult<ApplicationMutationResult>(*validationError);
                if (!m_services.snapshot || !m_services.applyGeneral)
                    return AutomationResult<ApplicationMutationResult>(unavailable());
                const auto current = m_services.snapshot().general;
                auto target = current;
                mutation(target);
                const auto validation = validateGeneral(target);
                if (!validation)
                    return AutomationResult<ApplicationMutationResult>(validation.getError());
                const bool changed = current != target;
                if (!validateOnly && changed && !m_services.applyGeneral(target))
                    return AutomationResult<ApplicationMutationResult>(persistenceError());
                return AutomationResult<ApplicationMutationResult>({
                    .changed = changed,
                    .validatedOnly = validateOnly,
                });
            });
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::updateGeneral(const ApplicationCommandContext &context,
                                                const GeneralSettingsDto &settings) {
        return update(
            OperationIds::settings::update_general, context, settings,
            [](const SettingsSnapshotDto &snapshot) { return snapshot.general; }, validateGeneral,
            m_services.applyGeneral);
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::updateAppearance(const ApplicationCommandContext &context,
                                                   const AppearanceSettingsDto &settings) {
        return update(
            OperationIds::settings::update_appearance, context, settings,
            [](const SettingsSnapshotDto &snapshot) { return snapshot.appearance; },
            validateAppearance, m_services.applyAppearance);
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::updateInference(const ApplicationCommandContext &context,
                                                  const InferenceSettingsDto &settings) {
        return update(
            OperationIds::settings::update_inference, context, settings,
            [](const SettingsSnapshotDto &snapshot) { return snapshot.inference; },
            validateInference, m_services.applyInference);
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::updateDeveloper(const ApplicationCommandContext &context,
                                                  const DeveloperSettingsDto &settings) {
        return update(
            OperationIds::settings::update_developer, context, settings,
            [](const SettingsSnapshotDto &snapshot) { return snapshot.developer; },
            validateDeveloper, m_services.applyDeveloper);
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::updateG2pLanguage(const ApplicationCommandContext &context,
                                                    const G2pLanguageSettingsDto &settings) {
        return update(
            OperationIds::settings::update_g2p_language, context, settings,
            [](const SettingsSnapshotDto &snapshot) { return snapshot.g2pLanguage; }, validateG2p,
            m_services.applyG2pLanguage);
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::updateFillLyric(const ApplicationCommandContext &context,
                                                  const FillLyricSettingsDto &settings) {
        return update(
            OperationIds::settings::update_fill_lyric, context, settings,
            [](const SettingsSnapshotDto &snapshot) { return snapshot.fillLyric; },
            [this](const FillLyricSettingsDto &target) { return validateFillLyricTarget(target); },
            m_services.applyFillLyric);
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::updateWindow(const ApplicationCommandContext &context,
                                               const WindowSettingsDto &settings) {
        return update(
            OperationIds::settings::update_window, context, settings,
            [](const SettingsSnapshotDto &snapshot) { return snapshot.window; }, validateWindow,
            m_services.applyWindow);
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::updateAudio(const ApplicationCommandContext &context,
                                              const AudioSettingsDto &settings) {
        return update(
            OperationIds::settings::update_audio, context, settings,
            [](const SettingsSnapshotDto &snapshot) { return snapshot.audio; }, validateAudio,
            m_services.applyAudio);
    }

    AutomationResult<QStringList> SettingsAutomationFacade::getRecentProjectFiles() {
        return m_dispatcher.dispatchApplicationQuery<QStringList>(
            OperationIds::recent_files::list, [this] {
                if (!m_services.snapshot)
                    return AutomationResult<QStringList>(unavailable());
                return AutomationResult<QStringList>(
                    m_services.snapshot().general.recentProjectFiles);
            });
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::addRecentProjectFile(const ApplicationCommandContext &context,
                                                       const QString &path) {
        std::optional<AutomationError> validationError;
        if (path.trimmed().isEmpty())
            validationError = AutomationError::invalidArgument(
                QStringLiteral("path"), QStringLiteral("Recent file path is empty"));
        return updateGeneralState(
            OperationIds::recent_files::add, context,
            [path](GeneralSettingsDto &general) {
                QStringList files{QDir::cleanPath(path.trimmed())};
                files.append(general.recentProjectFiles);
                general.recentProjectFiles = normalizedPaths(files, kMaximumRecentFiles);
            },
            std::move(validationError));
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::removeRecentProjectFile(const ApplicationCommandContext &context,
                                                          const QString &path) {
        std::optional<AutomationError> validationError;
        if (path.trimmed().isEmpty())
            validationError = AutomationError::invalidArgument(
                QStringLiteral("path"), QStringLiteral("Recent file path is empty"));
        return updateGeneralState(
            OperationIds::recent_files::remove, context,
            [path](GeneralSettingsDto &general) {
                const auto normalized = QDir::cleanPath(path.trimmed());
                general.recentProjectFiles.removeIf([&normalized](const QString &candidate) {
                    return pathsEqual(candidate, normalized);
                });
            },
            std::move(validationError));
    }

    AutomationResult<ApplicationMutationResult> SettingsAutomationFacade::clearRecentProjectFiles(
        const ApplicationCommandContext &context) {
        return updateGeneralState(
            OperationIds::recent_files::clear, context,
            [](GeneralSettingsDto &general) { general.recentProjectFiles.clear(); });
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::setPackageSearchPaths(const ApplicationCommandContext &context,
                                                        QStringList paths) {
        return updateGeneralState(OperationIds::packages::set_search_paths, context,
                                  [paths = std::move(paths)](GeneralSettingsDto &general) {
                                      general.packageSearchPaths = normalizedPaths(paths);
                                  });
    }

} // namespace Automation
