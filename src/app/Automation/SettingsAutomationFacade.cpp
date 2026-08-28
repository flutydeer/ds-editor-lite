#include "SettingsAutomationFacade.h"
#include "OperationIds.h"

#include "Model/AppOptions/Options/InferenceOption.h"

#include <QDir>

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

        AutomationResult<AutomationUnit> validateInference(const InferenceSettingsDto &settings) {
            if (settings.executionProvider != QStringLiteral("CPU") &&
                settings.executionProvider != QStringLiteral("DirectML") &&
                settings.executionProvider != QStringLiteral("CUDA")) {
                return AutomationError::invalidArgument(
                    QStringLiteral("execution_provider"),
                    QStringLiteral("Inference execution provider is unsupported"));
            }
            // A recognized provider can still be unusable on this platform/build
            // (e.g. DirectML on Linux); reject it instead of persisting a value
            // the engine cannot start with.
            if ((settings.executionProvider == QStringLiteral("DirectML") &&
                 !InferenceOption::directMlExecutionProviderAvailable()) ||
                (settings.executionProvider == QStringLiteral("CUDA") &&
                 !InferenceOption::cudaExecutionProviderAvailable())) {
                return AutomationError::invalidArgument(
                    QStringLiteral("execution_provider"),
                    QStringLiteral("Inference execution provider is unavailable on this "
                                   "platform/build"));
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
            for (const auto &rule : settings.customSplitterRules) {
                if (rule.name.trimmed().isEmpty() || splitterNames.contains(rule.name)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("custom_splitter_rules"),
                        QStringLiteral("Custom splitter rule names must be non-empty and unique"));
                }
                splitterNames.append(rule.name);
            }
            QStringList taggerLanguages;
            for (const auto &rule : settings.customTaggerRules) {
                if (rule.language.trimmed().isEmpty() || taggerLanguages.contains(rule.language)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("custom_tagger_rules"),
                        QStringLiteral("Custom tagger languages must be non-empty and unique"));
                }
                taggerLanguages.append(rule.language);
                for (const auto &entry : rule.entries) {
                    if (entry.type != QStringLiteral("regex") &&
                        entry.type != QStringLiteral("array") &&
                        entry.type != QStringLiteral("dict")) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("custom_tagger_rules.entries.type"),
                            QStringLiteral("Custom tagger entry type is unsupported"));
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
    }

    SettingsAutomationFacade::SettingsAutomationFacade(OperationCatalog &catalog,
                                                       AutomationDispatcher &dispatcher,
                                                       SettingsRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<SettingsSnapshotDto> SettingsAutomationFacade::getSettings() {
        return m_dispatcher.dispatchApplicationQuery<SettingsSnapshotDto>(
            OperationIds::settings::get, [this] {
                if (!m_services.snapshot)
                    return AutomationResult<SettingsSnapshotDto>(unavailable());
                return AutomationResult<SettingsSnapshotDto>(m_services.snapshot());
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
            validateFillLyric, m_services.applyFillLyric);
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

    AutomationResult<QStringList> SettingsAutomationFacade::getPackageSearchPaths() {
        return m_dispatcher.dispatchApplicationQuery<QStringList>(
            OperationIds::packages::get_search_paths, [this] {
                if (!m_services.snapshot)
                    return AutomationResult<QStringList>(unavailable());
                return AutomationResult<QStringList>(
                    m_services.snapshot().general.packageSearchPaths);
            });
    }

    AutomationResult<ApplicationMutationResult>
        SettingsAutomationFacade::setPackageSearchPaths(const ApplicationCommandContext &context,
                                                        QStringList paths) {
        return updateGeneralState(OperationIds::packages::set_search_paths, context,
                                  [paths = std::move(paths)](GeneralSettingsDto &general) {
                                      general.packageSearchPaths = normalizedPaths(paths);
                                  });
    }

    void SettingsAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        const auto addQuery = [&add](const OperationId &id, const QString &category) {
            add({
                .id = id,
                .category = category,
                .kind = OperationKind::Query,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::None,
                .revisionPolicy = RevisionPolicy::None,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::ReadOnly,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
        };
        const auto addCommand = [&add](const OperationId &id, const QString &category) {
            add({
                .id = id,
                .category = category,
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::None,
                .revisionPolicy = RevisionPolicy::None,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::Write,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
        };
        addQuery(OperationIds::settings::get, QStringLiteral("settings"));
        addQuery(OperationIds::recent_files::list, QStringLiteral("recent_files"));
        addQuery(OperationIds::packages::get_search_paths, QStringLiteral("packages"));
        addCommand(OperationIds::settings::update_general, QStringLiteral("settings"));
        addCommand(OperationIds::settings::update_appearance, QStringLiteral("settings"));
        addCommand(OperationIds::settings::update_inference, QStringLiteral("settings"));
        addCommand(OperationIds::settings::update_developer, QStringLiteral("settings"));
        addCommand(OperationIds::settings::update_g2p_language, QStringLiteral("settings"));
        addCommand(OperationIds::settings::update_fill_lyric, QStringLiteral("settings"));
        addCommand(OperationIds::settings::update_window, QStringLiteral("settings"));
        addCommand(OperationIds::settings::update_audio, QStringLiteral("settings"));
        addCommand(OperationIds::recent_files::add, QStringLiteral("recent_files"));
        addCommand(OperationIds::recent_files::remove, QStringLiteral("recent_files"));
        addCommand(OperationIds::recent_files::clear, QStringLiteral("recent_files"));
        addCommand(OperationIds::packages::set_search_paths, QStringLiteral("packages"));
    }

} // namespace Automation
