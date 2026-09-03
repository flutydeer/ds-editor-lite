#include "PublicAutomationRegistry.h"
#include "PublicAutomationCodecs.h"
#include "PublicCollectionPagination.h"

#include "../CoreRuntime.h"

#include <lite/AutomationWire/PublicConstants.h>

#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>

namespace Automation {

    namespace {

        namespace ToolNames = AutomationWire::PublicToolNames;

        QJsonArray stringArray(const QStringList &values) {
            QJsonArray result;
            for (const auto &value : values)
                result.append(value);
            return result;
        }

        QJsonArray integerArray(const QList<int> &values) {
            QJsonArray result;
            for (const auto value : values)
                result.append(value);
            return result;
        }

        QJsonArray integerArray(const QList<qint64> &values) {
            QJsonArray result;
            for (const auto value : values)
                result.append(value);
            return result;
        }

        QJsonArray numberArray(const QList<double> &values) {
            QJsonArray result;
            for (const auto value : values)
                result.append(value);
            return result;
        }

        QJsonArray availableCandidateIds(const QList<SettingsStringCandidateDto> &candidates) {
            QJsonArray result;
            for (const auto &candidate : candidates) {
                if (candidate.available)
                    result.append(candidate.id);
            }
            return result;
        }

        QJsonArray encodeDefaultLyrics(const QMap<QString, QString> &lyrics) {
            QJsonArray result;
            for (auto it = lyrics.cbegin(); it != lyrics.cend(); ++it) {
                result.append(QJsonObject{
                    {QStringLiteral("language_id"), it.key()  },
                    {QStringLiteral("lyric"),       it.value()},
                });
            }
            return result;
        }

        QMap<QString, QString> decodeDefaultLyrics(const QJsonArray &lyrics) {
            QMap<QString, QString> result;
            for (const auto &value : lyrics) {
                const auto object = value.toObject();
                result.insert(object.value(QStringLiteral("language_id")).toString(),
                              object.value(QStringLiteral("lyric")).toString());
            }
            return result;
        }

        QJsonObject encodeAudioValue(const AudioDevicePublicValueDto &value) {
            return {
                {QStringLiteral("driver_name"),                value.driverName             },
                {QStringLiteral("device_name"),                value.deviceName             },
                {QStringLiteral("buffer_size"),                value.bufferSize             },
                {QStringLiteral("sample_rate"),                value.sampleRate             },
                {QStringLiteral("hot_plug_notification_mode"), value.hotPlugNotificationMode},
                {QStringLiteral("gain"),                       value.gain                   },
                {QStringLiteral("pan"),                        value.pan                    },
            };
        }

        QJsonObject encodeComputeValue(const ComputeDevicePublicValueDto &value) {
            return {
                {QStringLiteral("execution_provider"), value.executionProvider                 },
                {QStringLiteral("gpu_index"),          value.gpuIndex                          },
                {QStringLiteral("gpu_id"),
                 value.gpuId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(value.gpuId)},
            };
        }

        QJsonObject encodeRenderValue(const RenderPublicValueDto &value) {
            return {
                {QStringLiteral("sampling_steps"),             value.samplingSteps           },
                {QStringLiteral("depth"),                      value.depth                   },
                {QStringLiteral("run_vocoder_on_cpu"),         value.runVocoderOnCpu         },
                {QStringLiteral("auto_start_inference"),       value.autoStartInference      },
                {QStringLiteral("playback_lookahead_seconds"), value.playbackLookaheadSeconds},
                {QStringLiteral("pitch_smooth_kernel_size"),   value.pitchSmoothKernelSize   },
            };
        }

        QJsonObject encodeRetentionValue(const SingerSessionRetentionPublicValueDto &value) {
            return {
                {QStringLiteral("capacity"),             value.capacity          },
                {QStringLiteral("idle_timeout_seconds"), value.idleTimeoutSeconds},
            };
        }

        QJsonObject encodeRange(const SettingsNumericRangeDto &range, const bool includeStep) {
            QJsonObject result{
                {QStringLiteral("minimum"), range.minimum},
                {QStringLiteral("maximum"), range.maximum},
            };
            if (includeStep)
                result.insert(QStringLiteral("step"), range.step);
            return result;
        }

        QJsonObject
            settingsDomainState(const QJsonValue &configured, const QJsonValue &effective,
                                const bool restartRequired, const QString &unavailableReason,
                                const QJsonValue &candidates = QJsonValue(QJsonValue::Undefined)) {
            QJsonObject result{
                {QStringLiteral("configured"),         configured                 },
                {QStringLiteral("effective"),          effective                  },
                {QStringLiteral("restart_required"),   restartRequired            },
                {QStringLiteral("available"),          unavailableReason.isEmpty()},
                {QStringLiteral("unavailable_reason"), unavailableReason          },
            };
            if (!candidates.isUndefined())
                result.insert(QStringLiteral("candidates"), candidates);
            return result;
        }

        QJsonObject encodePublicSettings(const PublicSettingsSnapshotDto &settings) {
            QJsonObject domains;
            if (settings.uiLanguage) {
                const auto &value = *settings.uiLanguage;
                domains.insert(QStringLiteral("ui_language"),
                               settingsDomainState(value.configured, value.effective, false,
                                                   value.unavailableReason,
                                                   availableCandidateIds(value.candidates)));
            }
            if (settings.singing) {
                const auto &value = *settings.singing;
                domains.insert(
                    QStringLiteral("singing"),
                    settingsDomainState(
                        QJsonObject{
                            {QStringLiteral("default_language"), value.configuredDefaultLanguage},
                            {QStringLiteral("default_lyrics"),
                             encodeDefaultLyrics(value.configuredDefaultLyrics)                 },
                },
                        QJsonObject{
                            {QStringLiteral("default_language"), value.effectiveDefaultLanguage},
                            {QStringLiteral("default_lyrics"),
                             encodeDefaultLyrics(value.effectiveDefaultLyrics)},
                        },
                        false, value.unavailableReason,
                        QJsonObject{
                            {QStringLiteral("languages"),
                             availableCandidateIds(value.languageCandidates)},
                        }));
            }
            if (settings.theme) {
                const auto &value = *settings.theme;
                domains.insert(QStringLiteral("theme"),
                               settingsDomainState(value.configured, value.effective, false,
                                                   value.unavailableReason,
                                                   availableCandidateIds(value.candidates)));
            }
            if (settings.audioDevice) {
                const auto &value = *settings.audioDevice;
                QJsonArray drivers;
                for (const auto &driver : value.drivers) {
                    if (!driver.available)
                        continue;
                    QJsonArray devices;
                    for (const auto &device : driver.devices) {
                        if (!device.available)
                            continue;
                        devices.append(QJsonObject{
                            {QStringLiteral("device_name"),  device.id                       },
                            {QStringLiteral("name"),         device.displayName              },
                            {QStringLiteral("buffer_sizes"), integerArray(device.bufferSizes)},
                            {QStringLiteral("sample_rates"), numberArray(device.sampleRates) },
                        });
                    }
                    drivers.append(QJsonObject{
                        {QStringLiteral("driver_name"), driver.id         },
                        {QStringLiteral("name"),        driver.displayName},
                        {QStringLiteral("devices"),     devices           },
                    });
                }
                domains.insert(
                    QStringLiteral("audio_device"),
                    settingsDomainState(
                        encodeAudioValue(value.configured), encodeAudioValue(value.effective),
                        false, value.unavailableReason,
                        QJsonObject{
                            {QStringLiteral("drivers"),                     drivers            },
                            {QStringLiteral("hot_plug_notification_modes"), QJsonArray{0, 1, 2}},
                            {QStringLiteral("gain_range"),
                             QJsonObject{
                                 {QStringLiteral("minimum"), 0.0},
                                 {QStringLiteral("maximum"),
                                  AutomationWire::MaximumAudioDeviceGain},
                             }                                                                 },
                            {QStringLiteral("pan_range"),
                             QJsonObject{
                                 {QStringLiteral("minimum"), -1.0},
                                 {QStringLiteral("maximum"), 1.0},
                             }                                                                 },
                }));
            }
            if (settings.playbackBehavior) {
                const auto &value = *settings.playbackBehavior;
                domains.insert(QStringLiteral("playback_behavior"),
                               settingsDomainState(value.configured, value.effective, false,
                                                   value.unavailableReason,
                                                   integerArray(value.candidates)));
            }
            if (settings.computeDevice) {
                const auto &value = *settings.computeDevice;
                QJsonArray gpus;
                for (const auto &gpu : value.gpuCandidates) {
                    if (!gpu.available || gpu.index < 0)
                        continue;
                    gpus.append(QJsonObject{
                        {QStringLiteral("index"), gpu.index      },
                        {QStringLiteral("id"),    gpu.id         },
                        {QStringLiteral("name"),  gpu.displayName},
                    });
                }
                domains.insert(QStringLiteral("compute_device"),
                               settingsDomainState(
                                   encodeComputeValue(value.configured),
                                   encodeComputeValue(value.effective),
                                   value.configured != value.effective, value.unavailableReason,
                                   QJsonObject{
                                       {QStringLiteral("execution_providers"),
                                        availableCandidateIds(value.providerCandidates)},
                                       {QStringLiteral("gpus"),                gpus    },
                }));
            }
            if (settings.render) {
                const auto &value = *settings.render;
                domains.insert(
                    QStringLiteral("render"),
                    settingsDomainState(
                        encodeRenderValue(value.configured), encodeRenderValue(value.effective),
                        value.configured.runVocoderOnCpu != value.effective.runVocoderOnCpu,
                        value.unavailableReason,
                        QJsonObject{
                            {QStringLiteral("sampling_steps"),
                             encodeRange(value.samplingStepsRange,                                       false)},
                            {QStringLiteral("depth"),                      encodeRange(value.depthRange, false)},
                            {QStringLiteral("playback_lookahead_seconds"),
                             encodeRange(value.playbackLookaheadRange,                                   false)},
                            {QStringLiteral("pitch_smooth_kernel_size"),
                             encodeRange(value.pitchSmoothKernelRange,                                   false)},
                }));
            }
            if (settings.singerSessionRetention) {
                const auto &value = *settings.singerSessionRetention;
                const SettingsNumericRangeDto capacityRange{
                    static_cast<double>(value.capacityCandidates.value(0)),
                    static_cast<double>(value.capacityCandidates.value(
                        std::max<qsizetype>(0, value.capacityCandidates.size() - 1))),
                    1.0,
                };
                const SettingsNumericRangeDto timeoutRange{
                    static_cast<double>(value.idleTimeoutCandidates.value(0)),
                    static_cast<double>(value.idleTimeoutCandidates.value(
                        std::max<qsizetype>(0, value.idleTimeoutCandidates.size() - 1))),
                    value.idleTimeoutCandidates.size() > 1
                        ? static_cast<double>(value.idleTimeoutCandidates.at(1) -
                                              value.idleTimeoutCandidates.at(0))
                        : 1.0,
                };
                domains.insert(
                    QStringLiteral("singer_session_retention"),
                    settingsDomainState(
                        encodeRetentionValue(value.configured),
                        encodeRetentionValue(value.effective), false, value.unavailableReason,
                        QJsonObject{
                            {QStringLiteral("capacity"),             encodeRange(capacityRange, false)},
                            {QStringLiteral("idle_timeout_seconds"),
                             encodeRange(timeoutRange,                                          true) },
                }));
            }
            if (settings.packageSearchPaths) {
                const auto &value = *settings.packageSearchPaths;
                domains.insert(
                    QStringLiteral("package_search_paths"),
                    settingsDomainState(
                        QJsonObject{
                            {QStringLiteral("paths"), stringArray(value.configured)}
                },
                        QJsonObject{{QStringLiteral("paths"), stringArray(value.effective)}},
                        value.configured != value.effective, value.unavailableReason));
            }
            return {
                {QStringLiteral("domains"), domains}
            };
        }

        QJsonObject encodeSettingsMutation(const SettingsMutationResultDto &mutation,
                                           const QJsonValue &configured,
                                           const QJsonValue &effective) {
            return {
                {QStringLiteral("changed"),                 mutation.changed        },
                {QStringLiteral("validated_only"),          mutation.validatedOnly  },
                {QStringLiteral("configured"),              configured              },
                {QStringLiteral("effective"),               effective               },
                {QStringLiteral("restart_required"),        mutation.restartRequired},
                {QStringLiteral("restart_required_fields"),
                 stringArray(mutation.restartRequiredFields)                        },
                {QStringLiteral("warnings"),                QJsonArray{}            },
            };
        }

        ApplicationCommandContext applicationContext(const QJsonObject &arguments,
                                                     const PublicInvocationContext &invocation) {
            return {
                .validateOnly = arguments.value(QStringLiteral("validate_only")).toBool(false),
                .source = invocation.source,
                .clientId = invocation.clientId,
            };
        }

        QStringList stringList(const QJsonArray &values) {
            QStringList result;
            result.reserve(values.size());
            for (const auto &value : values)
                result.append(value.toString());
            return result;
        }

        QJsonObject selectedSettingsDomain(const PublicSettingsSnapshotDto &settings,
                                           const QString &domain) {
            return encodePublicSettings(settings)
                .value(QStringLiteral("domains"))
                .toObject()
                .value(domain)
                .toObject();
        }

        QJsonObject encodeSettingsMutationWithState(const SettingsMutationResultDto &mutation,
                                                    const PublicSettingsSnapshotDto &settings,
                                                    const QString &domain) {
            const auto state = selectedSettingsDomain(settings, domain);
            return encodeSettingsMutation(mutation, state.value(QStringLiteral("configured")),
                                          state.value(QStringLiteral("effective")));
        }

        LyricRuleKind lyricRuleKind(const QString &kind) {
            return kind == QStringLiteral("tagger") ? LyricRuleKind::Tagger
                                                    : LyricRuleKind::Splitter;
        }

        QList<TaggerEntryDto> decodeTaggerEntries(const QJsonArray &entries) {
            QList<TaggerEntryDto> result;
            result.reserve(entries.size());
            for (const auto &value : entries) {
                const auto entry = value.toObject();
                result.append({
                    .type = entry.value(QStringLiteral("type")).toString(),
                    .value = stringList(entry.value(QStringLiteral("value")).toArray()),
                    .tag = entry.value(QStringLiteral("tag")).toString(),
                    .discard = entry.value(QStringLiteral("discard")).toBool(),
                });
            }
            return result;
        }

        QJsonArray encodeTaggerEntries(const QList<TaggerEntryDto> &entries) {
            QJsonArray result;
            for (const auto &entry : entries) {
                result.append(QJsonObject{
                    {QStringLiteral("type"),    entry.type              },
                    {QStringLiteral("value"),   stringArray(entry.value)},
                    {QStringLiteral("tag"),     entry.tag               },
                    {QStringLiteral("discard"), entry.discard           },
                });
            }
            return result;
        }

        QJsonObject encodeLyricRule(const LyricRuleDto &rule) {
            QJsonObject result{
                {QStringLiteral("rule_id"), rule.ruleId                                                          },
                {QStringLiteral("kind"),    rule.kind == LyricRuleKind::Tagger
                                             ? QStringLiteral("tagger")
                                             : QStringLiteral("splitter")},
                {QStringLiteral("builtin"), rule.builtin                                                         },
                {QStringLiteral("name"),    rule.name                                                            },
                {QStringLiteral("enabled"), rule.enabled                                                         },
                {QStringLiteral("order"),   rule.order                                                           },
            };
            if (rule.kind == LyricRuleKind::Splitter)
                result.insert(QStringLiteral("regexes"), stringArray(rule.regexes));
            else {
                result.insert(QStringLiteral("language"), rule.language);
                result.insert(QStringLiteral("entries"), encodeTaggerEntries(rule.entries));
            }
            return result;
        }

        QJsonObject encodeLyricRuleMutation(const LyricRuleMutationResultDto &mutation) {
            return {
                {QStringLiteral("changed"),        mutation.changed              },
                {QStringLiteral("validated_only"), mutation.validatedOnly        },
                {QStringLiteral("warnings"),       stringArray(mutation.warnings)},
                {QStringLiteral("rule"),           encodeLyricRule(mutation.rule)},
            };
        }

        QJsonObject encodeLyricRuleDeletion(const LyricRuleDeleteResultDto &mutation) {
            return {
                {QStringLiteral("changed"),        mutation.changed              },
                {QStringLiteral("validated_only"), mutation.validatedOnly        },
                {QStringLiteral("warnings"),       stringArray(mutation.warnings)},
                {QStringLiteral("rule_id"),        mutation.ruleId               },
            };
        }

        PackagePathProjection packagePathProjection(AutomationFileGuard &fileGuard) {
            return [&fileGuard](const QString &path) -> std::optional<QString> {
                const auto authorized = fileGuard.authorize(path, FileAccessPurpose::Read);
                if (!authorized)
                    return std::nullopt;
                return authorized.get().canonicalPath;
            };
        }

        bool containsAbsolutePathText(const QString &text) {
            static const QRegularExpression absolutePath(
                QStringLiteral(R"((?:^|[^A-Za-z0-9])(?:[A-Za-z]:[\\/]|\\\\|//|/(?!/)))"));
            return absolutePath.match(text).hasMatch();
        }

        QString sanitizePackageRefreshText(QString text, const QString &structuredPath = {}) {
            const auto sensitivity =
#ifdef Q_OS_WIN
                Qt::CaseInsensitive;
#else
                Qt::CaseSensitive;
#endif
            if ((!structuredPath.isEmpty() && text.contains(structuredPath, sensitivity)) ||
                containsAbsolutePathText(text)) {
                return QStringLiteral("Package refresh failure details were redacted");
            }
            return text;
        }

        PackageRefreshResultDto sanitizePackageRefreshResult(PackageRefreshResultDto result,
                                                             AutomationFileGuard &fileGuard) {
            for (auto &failure : result.failures) {
                const auto sourcePath = failure.path;
                if (sourcePath.isEmpty()) {
                    failure.path.clear();
                } else {
                    const auto authorized =
                        fileGuard.authorize(sourcePath, FileAccessPurpose::Read);
                    failure.path = authorized ? authorized.get().canonicalPath : QString();
                }
                failure.reason = sanitizePackageRefreshText(std::move(failure.reason), sourcePath);
            }
            return result;
        }

        AutomationError sanitizePackageRefreshError(AutomationError error) {
            error.message = sanitizePackageRefreshText(std::move(error.message));
            return error;
        }

        QJsonObject encodePackage(const PackageDto &package, const bool detailed) {
            QJsonArray voices;
            for (const auto &singer : package.singers) {
                QJsonArray languages;
                for (const auto &language : singer.info.languages())
                    languages.append(language.id());
                QJsonArray speakers;
                for (const auto &speaker : singer.info.speakers())
                    speakers.append(speaker.id());
                QJsonObject voice{
                    {QStringLiteral("singer_id"), singer.singerId},
                    {QStringLiteral("name"), singer.name},
                    {QStringLiteral("display_name"),
                     resolvePublicDisplayText(singer.name, singer.info.localizedNames())},
                    {QStringLiteral("languages"), languages},
                    {QStringLiteral("speakers"), speakers},
                };
                if (detailed) {
                    voice.insert(QStringLiteral("localized_names"),
                                 encodePublicLocalizedText(singer.info.localizedNames()));
                    voice.insert(QStringLiteral("description"), QString());
                    voice.insert(QStringLiteral("avatar_path"), QString());
                }
                voices.append(voice);
            }
            QJsonObject result{
                {QStringLiteral("package_id"), package.id},
                {QStringLiteral("version"), package.version.toString()},
                {QStringLiteral("vendor"), package.vendor},
                {QStringLiteral("display_vendor"),
                 resolvePublicDisplayText(package.vendor, package.localizedVendor)},
                {QStringLiteral("canonical_path"),
                 package.path.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(package.path)},
                {QStringLiteral("voices"), voices},
            };
            if (detailed) {
                result.insert(QStringLiteral("localized_vendor"),
                              encodePublicLocalizedText(package.localizedVendor));
                result.insert(QStringLiteral("description"), package.description);
                result.insert(
                    QStringLiteral("display_description"),
                    resolvePublicDisplayText(package.description, package.localizedDescription));
                result.insert(QStringLiteral("localized_description"),
                              encodePublicLocalizedText(package.localizedDescription));
                result.insert(QStringLiteral("license"), package.license);
                result.insert(QStringLiteral("display_license"),
                              resolvePublicDisplayText(package.license, package.localizedLicense));
                result.insert(QStringLiteral("localized_license"),
                              encodePublicLocalizedText(package.localizedLicense));
                result.insert(QStringLiteral("homepage"), package.url);
            }
            return result;
        }

        QJsonObject encodePackageRefresh(const PackageRefreshResultDto &refresh) {
            QJsonArray failures;
            for (const auto &failure : refresh.failures) {
                failures.append(QJsonObject{
                    {QStringLiteral("path"),   failure.path.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                                    : QJsonValue(failure.path)},
                    {QStringLiteral("reason"), failure.reason                                                                             },
                });
            }
            return {
                {QStringLiteral("packages"), refresh.packages            },
                {QStringLiteral("added"),    stringArray(refresh.added)  },
                {QStringLiteral("updated"),  stringArray(refresh.updated)},
                {QStringLiteral("removed"),  stringArray(refresh.removed)},
                {QStringLiteral("failures"), failures                    },
            };
        }

        QJsonObject encodeApplicationTaskAccepted(const TaskAcceptedResult &accepted) {
            QJsonObject result{
                {QStringLiteral("scope"),          QStringLiteral("application")},
                {QStringLiteral("document"),       QJsonValue(QJsonValue::Null) },
                {QStringLiteral("validated_only"), accepted.validatedOnly       },
            };
            if (!accepted.validatedOnly)
                result.insert(QStringLiteral("task_id"), accepted.taskId.toString());
            return result;
        }

        AutomationResult<PublicSettingsSnapshotDto>
            queryPublicSettings(CoreRuntime &runtime, AutomationFileGuard &fileGuard,
                                QStringList domains = {}) {
            return runtime.settings().queryPublicSettings(
                std::move(domains), [&fileGuard](const QString &path) -> std::optional<QString> {
                    const auto authorized = fileGuard.authorize(path, FileAccessPurpose::Read);
                    if (!authorized)
                        return std::nullopt;
                    return authorized.get().canonicalPath;
                });
        }

        template <typename Patch, typename Update>
        AutomationResult<QJsonObject>
            updateSetting(CoreRuntime &runtime, AutomationFileGuard &fileGuard,
                          const QJsonObject &arguments, const PublicInvocationContext &invocation,
                          const QString &domain, Patch patch, Update update) {
            auto mutation = update(runtime.settings(), applicationContext(arguments, invocation),
                                   std::move(patch));
            if (!mutation)
                return mutation.getError();
            auto settings = queryPublicSettings(runtime, fileGuard, {domain});
            if (!settings)
                return settings.getError();
            return encodeSettingsMutationWithState(mutation.get(), settings.get(), domain);
        }

    } // namespace

    void PublicAutomationRegistry::registerAdvancedApplicationBindings() {
        addBinding(ToolNames::settings_query,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto settings = queryPublicSettings(
                           m_runtime, m_fileGuard,
                           stringList(arguments.value(QStringLiteral("domains")).toArray()));
                       if (!settings)
                           return AutomationResult<QJsonObject>(settings.getError());
                       return AutomationResult<QJsonObject>(encodePublicSettings(settings.get()));
                   });

        addBinding(ToolNames::settings_ui_language_update,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       UiLanguageSettingsPatchDto patch;
                       if (arguments.contains(QStringLiteral("ui_language"))) {
                           patch.uiLanguage =
                               arguments.value(QStringLiteral("ui_language")).toString();
                       }
                       return updateSetting(m_runtime, m_fileGuard, arguments, invocation,
                                            QStringLiteral("ui_language"), std::move(patch),
                                            [](SettingsAutomationFacade &settings,
                                               const ApplicationCommandContext &context,
                                               const UiLanguageSettingsPatchDto &value) {
                                                return settings.updateUiLanguage(context, value);
                                            });
                   });
        addBinding(ToolNames::settings_singing_update,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       SingingSettingsPatchDto patch;
                       if (arguments.contains(QStringLiteral("default_language"))) {
                           patch.defaultLanguage =
                               arguments.value(QStringLiteral("default_language")).toString();
                       }
                       if (arguments.contains(QStringLiteral("default_lyrics"))) {
                           patch.defaultLyrics = decodeDefaultLyrics(
                               arguments.value(QStringLiteral("default_lyrics")).toArray());
                       }
                       return updateSetting(m_runtime, m_fileGuard, arguments, invocation,
                                            QStringLiteral("singing"), std::move(patch),
                                            [](SettingsAutomationFacade &settings,
                                               const ApplicationCommandContext &context,
                                               const SingingSettingsPatchDto &value) {
                                                return settings.updateSinging(context, value);
                                            });
                   });
        addBinding(ToolNames::settings_theme_update,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       ThemeSettingsPatchDto patch;
                       if (arguments.contains(QStringLiteral("theme_id")))
                           patch.themeId = arguments.value(QStringLiteral("theme_id")).toString();
                       return updateSetting(m_runtime, m_fileGuard, arguments, invocation,
                                            QStringLiteral("theme"), std::move(patch),
                                            [](SettingsAutomationFacade &settings,
                                               const ApplicationCommandContext &context,
                                               const ThemeSettingsPatchDto &value) {
                                                return settings.updateTheme(context, value);
                                            });
                   });
        addBinding(
            ToolNames::settings_audio_device_update,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                AudioDeviceSettingsPatchDto patch;
                if (arguments.contains(QStringLiteral("driver_name"))) {
                    patch.driverName = arguments.value(QStringLiteral("driver_name")).toString();
                }
                if (arguments.contains(QStringLiteral("device_name"))) {
                    patch.deviceName = arguments.value(QStringLiteral("device_name")).toString();
                }
                if (arguments.contains(QStringLiteral("buffer_size"))) {
                    patch.bufferSize = arguments.value(QStringLiteral("buffer_size")).toInteger();
                }
                if (arguments.contains(QStringLiteral("sample_rate"))) {
                    patch.sampleRate = arguments.value(QStringLiteral("sample_rate")).toDouble();
                }
                if (arguments.contains(QStringLiteral("hot_plug_notification_mode"))) {
                    patch.hotPlugNotificationMode =
                        arguments.value(QStringLiteral("hot_plug_notification_mode")).toInt();
                }
                if (arguments.contains(QStringLiteral("gain")))
                    patch.gain = arguments.value(QStringLiteral("gain")).toDouble();
                if (arguments.contains(QStringLiteral("pan")))
                    patch.pan = arguments.value(QStringLiteral("pan")).toDouble();
                return updateSetting(m_runtime, m_fileGuard, arguments, invocation,
                                     QStringLiteral("audio_device"), std::move(patch),
                                     [](SettingsAutomationFacade &settings,
                                        const ApplicationCommandContext &context,
                                        const AudioDeviceSettingsPatchDto &value) {
                                         return settings.updateAudioDevice(context, value);
                                     });
            });
        addBinding(ToolNames::settings_playback_behavior_update,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       PlaybackBehaviorSettingsPatchDto patch;
                       if (arguments.contains(QStringLiteral("behavior")))
                           patch.behavior = arguments.value(QStringLiteral("behavior")).toInt();
                       return updateSetting(m_runtime, m_fileGuard, arguments, invocation,
                                            QStringLiteral("playback_behavior"), std::move(patch),
                                            [](SettingsAutomationFacade &settings,
                                               const ApplicationCommandContext &context,
                                               const PlaybackBehaviorSettingsPatchDto &value) {
                                                return settings.updatePlaybackBehavior(context,
                                                                                       value);
                                            });
                   });
        addBinding(ToolNames::settings_compute_device_update,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       ComputeDeviceSettingsPatchDto patch;
                       if (arguments.contains(QStringLiteral("execution_provider"))) {
                           patch.executionProvider =
                               arguments.value(QStringLiteral("execution_provider")).toString();
                       }
                       if (arguments.contains(QStringLiteral("gpu_index")))
                           patch.gpuIndex = arguments.value(QStringLiteral("gpu_index")).toInt();
                       if (arguments.contains(QStringLiteral("gpu_id"))) {
                           patch.gpuId = arguments.value(QStringLiteral("gpu_id")).isNull()
                                             ? QString()
                                             : arguments.value(QStringLiteral("gpu_id")).toString();
                       }
                       return updateSetting(m_runtime, m_fileGuard, arguments, invocation,
                                            QStringLiteral("compute_device"), std::move(patch),
                                            [](SettingsAutomationFacade &settings,
                                               const ApplicationCommandContext &context,
                                               const ComputeDeviceSettingsPatchDto &value) {
                                                return settings.updateComputeDevice(context, value);
                                            });
                   });
        addBinding(
            ToolNames::settings_render_update,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                RenderSettingsPatchDto patch;
                if (arguments.contains(QStringLiteral("sampling_steps"))) {
                    patch.samplingSteps = arguments.value(QStringLiteral("sampling_steps")).toInt();
                }
                if (arguments.contains(QStringLiteral("depth")))
                    patch.depth = arguments.value(QStringLiteral("depth")).toDouble();
                if (arguments.contains(QStringLiteral("run_vocoder_on_cpu"))) {
                    patch.runVocoderOnCpu =
                        arguments.value(QStringLiteral("run_vocoder_on_cpu")).toBool();
                }
                if (arguments.contains(QStringLiteral("auto_start_inference"))) {
                    patch.autoStartInference =
                        arguments.value(QStringLiteral("auto_start_inference")).toBool();
                }
                if (arguments.contains(QStringLiteral("playback_lookahead_seconds"))) {
                    patch.playbackLookaheadSeconds =
                        arguments.value(QStringLiteral("playback_lookahead_seconds")).toDouble();
                }
                if (arguments.contains(QStringLiteral("pitch_smooth_kernel_size"))) {
                    patch.pitchSmoothKernelSize =
                        arguments.value(QStringLiteral("pitch_smooth_kernel_size")).toInt();
                }
                return updateSetting(m_runtime, m_fileGuard, arguments, invocation,
                                     QStringLiteral("render"), std::move(patch),
                                     [](SettingsAutomationFacade &settings,
                                        const ApplicationCommandContext &context,
                                        const RenderSettingsPatchDto &value) {
                                         return settings.updateRender(context, value);
                                     });
            });
        addBinding(ToolNames::settings_singer_session_retention_update,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       SingerSessionRetentionSettingsPatchDto patch;
                       if (arguments.contains(QStringLiteral("capacity")))
                           patch.capacity = arguments.value(QStringLiteral("capacity")).toInt();
                       if (arguments.contains(QStringLiteral("idle_timeout_seconds"))) {
                           patch.idleTimeoutSeconds =
                               arguments.value(QStringLiteral("idle_timeout_seconds")).toInt();
                       }
                       return updateSetting(
                           m_runtime, m_fileGuard, arguments, invocation,
                           QStringLiteral("singer_session_retention"), std::move(patch),
                           [](SettingsAutomationFacade &settings,
                              const ApplicationCommandContext &context,
                              const SingerSessionRetentionSettingsPatchDto &value) {
                               return settings.updateSingerSessionRetention(context, value);
                           });
                   });
        addBinding(ToolNames::packages_list, [this](const QJsonObject &arguments,
                                                    const PublicInvocationContext &) {
            auto packages =
                m_runtime.packages().getInstalledPackages(packagePathProjection(m_fileGuard));
            if (!packages)
                return AutomationResult<QJsonObject>(packages.getError());
            const auto query = arguments.value(QStringLiteral("query")).toString().trimmed();
            auto values = packages.get();
            std::sort(values.begin(), values.end(), [](const auto &left, const auto &right) {
                if (left.id != right.id)
                    return left.id < right.id;
                return QVersionNumber::compare(left.version, right.version) < 0;
            });
            QJsonArray encoded;
            for (const auto &package : values) {
                bool matches =
                    package.id.contains(query, Qt::CaseInsensitive) ||
                    publicLocalizedTextContains(query, package.vendor, package.localizedVendor);
                if (!matches) {
                    matches = std::any_of(
                        package.singers.cbegin(), package.singers.cend(),
                        [&query](const auto &singer) {
                            return singer.singerId.contains(query, Qt::CaseInsensitive) ||
                                   publicLocalizedTextContains(query, singer.name,
                                                               singer.info.localizedNames());
                        });
                }
                if (matches)
                    encoded.append(encodePackage(package, false));
            }
            auto page =
                PublicRegistryDetail::paginateJson(m_collectionCursorCodec, encoded, arguments,
                                                   QStringLiteral("editor-public-packages/v1"),
                                                   QJsonObject{
                                                       {QStringLiteral("query"),    query  },
                                                       {QStringLiteral("packages"), encoded},
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            QJsonObject result{
                {QStringLiteral("packages"), page.get().items}
            };
            if (!page.get().nextCursor.isEmpty()) {
                result.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            }
            return AutomationResult<QJsonObject>(std::move(result));
        });
        addBinding(ToolNames::packages_describe,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto package = m_runtime.packages().describePackage(
                           arguments.value(QStringLiteral("package_id")).toString(),
                           arguments.value(QStringLiteral("version")).toString(),
                           packagePathProjection(m_fileGuard));
                       if (!package)
                           return AutomationResult<QJsonObject>(package.getError());
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("package"), encodePackage(package.get(), true)},
                       });
                   });
        addBinding(ToolNames::packages_refresh, [this](const QJsonObject &arguments,
                                                       const PublicInvocationContext &invocation) {
            const auto context = applicationContext(arguments, invocation);
            if (context.validateOnly) {
                auto validation = m_runtime.packages().refreshPackages(
                    context, [](AutomationResult<PackageRefreshResultDto>) {});
                if (!validation)
                    return AutomationResult<QJsonObject>(validation.getError());
                return AutomationResult<QJsonObject>(encodeApplicationTaskAccepted(
                    {{}, {}, true, AutomationTaskScope::Application}));
            }

            auto snapshot = m_runtime.automationTasks().createApplicationTask(
                QString(ToolNames::packages_refresh), [] {}, invocation.clientId);
            m_runtime.automationTasks().markRunning(snapshot.taskId);
            const auto lifetime = std::weak_ptr<LifetimeState>(m_lifetimeState);
            auto started = m_runtime.packages().refreshPackages(
                context,
                [lifetime, runtime = &m_runtime, fileGuard = &m_fileGuard,
                 taskId =
                     snapshot.taskId](AutomationResult<PackageRefreshResultDto> result) mutable {
                    const auto state = lifetime.lock();
                    if (!state || !state->active.load(std::memory_order_acquire))
                        return;
                    if (!result) {
                        if (runtime->automationTasks().isCancellationRequested(taskId)) {
                            runtime->automationTasks().cancel(taskId);
                            return;
                        }
                        runtime->automationTasks().fail(
                            taskId, sanitizePackageRefreshError(result.getError()));
                        return;
                    }
                    auto sanitized =
                        sanitizePackageRefreshResult(std::move(result.get()), *fileGuard);
                    runtime->automationTasks().succeedApplication(taskId,
                                                                  encodePackageRefresh(sanitized));
                },
                {},
                [lifetime, runtime = &m_runtime, taskId = snapshot.taskId] {
                    const auto state = lifetime.lock();
                    if (!state || !state->active.load(std::memory_order_acquire))
                        return false;
                    const auto committing = runtime->automationTasks().beginCommitting(taskId);
                    return committing && committing.get();
                });
            if (!started)
                m_runtime.automationTasks().fail(snapshot.taskId,
                                                 sanitizePackageRefreshError(started.getError()));
            return AutomationResult<QJsonObject>(encodeApplicationTaskAccepted(
                {snapshot.taskId, {}, false, AutomationTaskScope::Application}));
        });

        addBinding(ToolNames::lyric_rules_list, [this](const QJsonObject &arguments,
                                                       const PublicInvocationContext &) {
            auto rules = m_runtime.settings().listLyricRules();
            if (!rules)
                return AutomationResult<QJsonObject>(rules.getError());
            const auto kindFilter = arguments.value(QStringLiteral("kind")).toString();
            const auto includeDisabled =
                arguments.value(QStringLiteral("include_disabled")).toBool(false);
            QJsonArray encoded;
            for (const auto &rule : rules.get()) {
                const auto kind = rule.kind == LyricRuleKind::Tagger ? QStringLiteral("tagger")
                                                                     : QStringLiteral("splitter");
                if (!kindFilter.isEmpty() && kind != kindFilter)
                    continue;
                if (!includeDisabled && !rule.enabled)
                    continue;
                encoded.append(encodeLyricRule(rule));
            }
            return AutomationResult<QJsonObject>(QJsonObject{
                {QStringLiteral("rules"), encoded}
            });
        });
        addBinding(
            ToolNames::lyric_rules_create,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                LyricRuleDraftDto draft{
                    .kind = lyricRuleKind(arguments.value(QStringLiteral("kind")).toString()),
                    .name = arguments.value(QStringLiteral("name")).toString(),
                    .language = arguments.value(QStringLiteral("language")).toString(),
                    .regexes = stringList(arguments.value(QStringLiteral("regexes")).toArray()),
                    .entries =
                        decodeTaggerEntries(arguments.value(QStringLiteral("entries")).toArray()),
                    .enabled = arguments.value(QStringLiteral("enabled")).toBool(true),
                };
                if (arguments.contains(QStringLiteral("position")))
                    draft.position = arguments.value(QStringLiteral("position")).toInt();
                auto result = m_runtime.settings().createLyricRule(
                    applicationContext(arguments, invocation), draft);
                if (!result)
                    return AutomationResult<QJsonObject>(result.getError());
                return AutomationResult<QJsonObject>(encodeLyricRuleMutation(result.get()));
            });
        addBinding(ToolNames::lyric_rules_update,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       LyricRulePatchDto patch;
                       if (arguments.contains(QStringLiteral("name")))
                           patch.name = arguments.value(QStringLiteral("name")).toString();
                       if (arguments.contains(QStringLiteral("language")))
                           patch.language = arguments.value(QStringLiteral("language")).toString();
                       if (arguments.contains(QStringLiteral("regexes"))) {
                           patch.regexes =
                               stringList(arguments.value(QStringLiteral("regexes")).toArray());
                       }
                       if (arguments.contains(QStringLiteral("entries"))) {
                           patch.entries = decodeTaggerEntries(
                               arguments.value(QStringLiteral("entries")).toArray());
                       }
                       auto result = m_runtime.settings().updateLyricRule(
                           applicationContext(arguments, invocation),
                           arguments.value(QStringLiteral("rule_id")).toString(), patch);
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(encodeLyricRuleMutation(result.get()));
                   });
        addBinding(ToolNames::lyric_rules_delete,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto result = m_runtime.settings().deleteLyricRule(
                           applicationContext(arguments, invocation),
                           arguments.value(QStringLiteral("rule_id")).toString());
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(encodeLyricRuleDeletion(result.get()));
                   });
        addBinding(ToolNames::lyric_rules_set_enabled,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto result = m_runtime.settings().setLyricRuleEnabled(
                           applicationContext(arguments, invocation),
                           arguments.value(QStringLiteral("rule_id")).toString(),
                           arguments.value(QStringLiteral("enabled")).toBool());
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(encodeLyricRuleMutation(result.get()));
                   });
        addBinding(ToolNames::lyric_rules_move,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto result = m_runtime.settings().moveLyricRule(
                           applicationContext(arguments, invocation),
                           arguments.value(QStringLiteral("rule_id")).toString(),
                           arguments.value(QStringLiteral("position")).toInt());
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(encodeLyricRuleMutation(result.get()));
                   });
        addBinding(ToolNames::lyric_rules_test,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto result = m_runtime.settings().testLyricRules(
                           arguments.value(QStringLiteral("text")).toString());
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       QJsonArray taggedTokens;
                       for (const auto &token : result.get().taggedTokens) {
                           taggedTokens.append(QJsonObject{
                               {QStringLiteral("lyric"),    token.lyric   },
                               {QStringLiteral("language"), token.language},
                               {QStringLiteral("tag"),      token.tag     },
                               {QStringLiteral("discard"),  token.discard },
                           });
                       }
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("split_tokens"),  stringArray(result.get().splitTokens)},
                           {QStringLiteral("tagged_tokens"), taggedTokens                         },
                       });
                   });
    }

} // namespace Automation
