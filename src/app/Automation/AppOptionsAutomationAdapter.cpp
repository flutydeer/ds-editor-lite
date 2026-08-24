#include "AppOptionsAutomationAdapter.h"

#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioSettings.h"

#include <QJsonArray>
#include <QJsonObject>

namespace Automation {
    namespace {
        constexpr int kSpeakerMixPresetSchemaVersion = 1;

        GeneralSettingsDto captureGeneral(AppOptions *options) {
            const auto *value = options->general();
            return {
                .uiLanguage = value->uiLanguage,
                .defaultSingingLanguage = value->defaultSingingLanguage,
                .defaultLyrics = value->defaultLyrics,
                .packageSearchPaths = value->packageSearchPaths,
                .recentProjectFiles = value->recentProjectFiles,
                .gameDirectory = value->gameDir,
                .pitchModelPath = value->rmvpePath,
                .libreSvipPath = value->libreSVIPPath,
            };
        }

        void restoreGeneral(AppOptions *options, const GeneralSettingsDto &value) {
            auto *target = options->general();
            target->uiLanguage = value.uiLanguage;
            target->defaultSingingLanguage = value.defaultSingingLanguage;
            target->defaultLyrics = value.defaultLyrics;
            target->packageSearchPaths = value.packageSearchPaths;
            target->recentProjectFiles = value.recentProjectFiles;
            target->gameDir = value.gameDirectory;
            target->rmvpePath = value.pitchModelPath;
            target->libreSVIPPath = value.libreSvipPath;
        }

        AppearanceSettingsDto captureAppearance(AppOptions *options) {
            const auto *value = options->appearance();
            return {
                .useNativeFrame = value->useNativeFrame,
                .enableDirectManipulation = value->enableDirectManipulation,
                .animationEnabled = value->animationEnabled,
                .animationTimeScale = value->animationTimeScale,
                .themeId = value->themeId,
                .uiFontFamily = value->uiFontFamily,
            };
        }

        void restoreAppearance(AppOptions *options, const AppearanceSettingsDto &value) {
            auto *target = options->appearance();
            target->useNativeFrame = value.useNativeFrame;
            target->enableDirectManipulation = value.enableDirectManipulation;
            target->animationEnabled = value.animationEnabled;
            target->animationTimeScale = value.animationTimeScale;
            target->themeId = value.themeId;
            target->uiFontFamily = value.uiFontFamily;
        }

        InferenceSettingsDto captureInference(AppOptions *options) {
            const auto *value = options->inference();
            return {
                .executionProvider = value->executionProvider,
                .selectedGpuIndex = value->selectedGpuIndex,
                .selectedGpuId = value->selectedGpuId,
                .samplingSteps = value->samplingSteps,
                .depth = value->depth,
                .runVocoderOnCpu = value->runVocoderOnCpu,
                .autoStartInference = value->autoStartInfer,
                .playbackLookaheadSeconds = value->playbackLookaheadSeconds,
                .cacheDirectory = value->cacheDirectory,
                .singerSessionCacheCapacity = value->singerSessionCacheCapacity,
                .singerSessionIdleTimeoutSeconds = value->singerSessionIdleTimeoutSeconds,
                .pitchSmoothKernelSize = value->pitch_smooth_kernel_size,
            };
        }

        void restoreInference(AppOptions *options, const InferenceSettingsDto &value) {
            auto *target = options->inference();
            target->executionProvider = value.executionProvider;
            target->selectedGpuIndex = value.selectedGpuIndex;
            target->selectedGpuId = value.selectedGpuId;
            target->samplingSteps = value.samplingSteps;
            target->depth = value.depth;
            target->runVocoderOnCpu = value.runVocoderOnCpu;
            target->autoStartInfer = value.autoStartInference;
            target->playbackLookaheadSeconds = value.playbackLookaheadSeconds;
            target->cacheDirectory = value.cacheDirectory;
            target->singerSessionCacheCapacity = value.singerSessionCacheCapacity;
            target->singerSessionIdleTimeoutSeconds = value.singerSessionIdleTimeoutSeconds;
            target->pitch_smooth_kernel_size = value.pitchSmoothKernelSize;
        }

        DeveloperSettingsDto captureDeveloper(AppOptions *options) {
            const auto *value = options->developer();
            return {
                .enableDiagnostics = value->enableDiagnostics,
                .showLogWindow = value->showLogWindow,
                .showTimelineDebugInfo = value->showTimelineDebugInfo,
                .showClipDebugInfo = value->showClipDebugInfo,
                .enablePanelDetach = value->enablePanelDetach,
                .enableEmbeddedOptionsDialog = value->enableEmbeddedOptionsDialog,
                .editorRenderBackend = value->editorRenderBackend ==
                                               DeveloperOption::EditorRenderBackend::RhiExperimental
                                           ? EditorRenderBackend::RhiExperimental
                                           : EditorRenderBackend::Legacy,
            };
        }

        void restoreDeveloper(AppOptions *options, const DeveloperSettingsDto &value) {
            auto *target = options->developer();
            target->enableDiagnostics = value.enableDiagnostics;
            target->showLogWindow = value.showLogWindow;
            target->showTimelineDebugInfo = value.showTimelineDebugInfo;
            target->showClipDebugInfo = value.showClipDebugInfo;
            target->enablePanelDetach = value.enablePanelDetach;
            target->enableEmbeddedOptionsDialog = value.enableEmbeddedOptionsDialog;
            target->editorRenderBackend =
                value.editorRenderBackend == EditorRenderBackend::RhiExperimental
                    ? DeveloperOption::EditorRenderBackend::RhiExperimental
                    : DeveloperOption::EditorRenderBackend::Legacy;
        }

        G2pLanguageSettingsDto captureG2pLanguage(AppOptions *options) {
            return {.languageOrder = options->g2pLanguage()->langOrder};
        }

        void restoreG2pLanguage(AppOptions *options, const G2pLanguageSettingsDto &value) {
            options->g2pLanguage()->langOrder = value.languageOrder;
        }

        FillLyricSettingsDto captureFillLyric(AppOptions *options) {
            const auto *value = options->fillLyric();
            FillLyricSettingsDto result{
                .baseVisible = value->baseVisible,
                .extensionVisible = value->extVisible,
                .splitMode = value->splitMode,
                .skipSlur = value->skipSlur,
                .exportLanguage = value->exportLanguage,
                .textEditFontSize = value->textEditFontSize,
                .viewFontSize = value->viewFontSize,
                .builtinSplitterEnabled = value->builtinSplitterEnabled,
                .builtinTaggerEnabled = value->builtinTaggerEnabled,
                .splitterOrder = value->splitterOrder,
                .taggerOrder = value->taggerOrder,
            };
            for (const auto &rule : value->customSplitterRules) {
                result.customSplitterRules.append({
                    .name = rule.name,
                    .regexes = rule.regexes,
                    .enabled = rule.enabled,
                    .order = rule.order,
                });
            }
            for (const auto &rule : value->customTaggerRules) {
                TaggerRuleDto converted{
                    .language = rule.language,
                    .enabled = rule.enabled,
                };
                for (const auto &entry : rule.entries) {
                    converted.entries.append({
                        .type = entry.type,
                        .value = entry.value,
                        .tag = entry.tag,
                        .discard = entry.discard,
                    });
                }
                result.customTaggerRules.append(std::move(converted));
            }
            return result;
        }

        void restoreFillLyric(AppOptions *options, const FillLyricSettingsDto &value) {
            auto *target = options->fillLyric();
            target->baseVisible = value.baseVisible;
            target->extVisible = value.extensionVisible;
            target->splitMode = value.splitMode;
            target->skipSlur = value.skipSlur;
            target->exportLanguage = value.exportLanguage;
            target->textEditFontSize = value.textEditFontSize;
            target->viewFontSize = value.viewFontSize;
            target->builtinSplitterEnabled = value.builtinSplitterEnabled;
            target->builtinTaggerEnabled = value.builtinTaggerEnabled;
            target->splitterOrder = value.splitterOrder;
            target->taggerOrder = value.taggerOrder;
            target->customSplitterRules.clear();
            for (const auto &rule : value.customSplitterRules) {
                target->customSplitterRules.append({
                    .name = rule.name,
                    .regexes = rule.regexes,
                    .enabled = rule.enabled,
                    .order = rule.order,
                });
            }
            target->customTaggerRules.clear();
            for (const auto &rule : value.customTaggerRules) {
                CustomTaggerRule converted{
                    .language = rule.language,
                    .enabled = rule.enabled,
                };
                for (const auto &entry : rule.entries) {
                    converted.entries.append({
                        .type = entry.type,
                        .value = entry.value,
                        .tag = entry.tag,
                        .discard = entry.discard,
                    });
                }
                target->customTaggerRules.append(std::move(converted));
            }
        }

        WindowSettingsDto captureWindow(AppOptions *options) {
            return {.mainWindowGeometry = options->window()->mainWindowGeometry()};
        }

        AudioSettingsDto captureAudio(AppOptions *options) {
            AudioSettingsDto result{
                .adoptedBufferSize = AudioSettings::adoptedBufferSize(),
                .adoptedSampleRate = AudioSettings::adoptedSampleRate(),
                .deviceGain = AudioSettings::deviceGain(),
                .deviceName = AudioSettings::deviceName(),
                .devicePan = AudioSettings::devicePan(),
                .driverName = AudioSettings::driverName(),
                .fileBufferingReadAheadSize = AudioSettings::fileBufferingReadAheadSize(),
                .hotPlugNotificationMode = AudioSettings::hotPlugNotificationMode(),
                .playheadBehavior = AudioSettings::playheadBehavior(),
                .midiDeviceIndex = AudioSettings::midiDeviceIndex(),
                .midiSynthesizerAmplitude = AudioSettings::midiSynthesizerAmplitude(),
                .midiSynthesizerAttackMilliseconds = AudioSettings::midiSynthesizerAttackMsec(),
                .midiSynthesizerDecayMilliseconds = AudioSettings::midiSynthesizerDecayMsec(),
                .midiSynthesizerDecayRatio = AudioSettings::midiSynthesizerDecayRatio(),
                .midiSynthesizerFrequencyOfA = AudioSettings::midiSynthesizerFrequencyOfA(),
                .midiSynthesizerGenerator = AudioSettings::midiSynthesizerGenerator(),
                .midiSynthesizerReleaseMilliseconds = AudioSettings::midiSynthesizerReleaseMsec(),
                .pseudoSingerReadEnergy = AudioSettings::pseudoSingerReadEnergy(),
                .pseudoSingerReadPitch = AudioSettings::pseudoSingerReadPitch(),
                .vstEditorPort = AudioSettings::vstEditorPort(),
                .vstPluginEditorUsesCustomTheme = AudioSettings::vstPluginEditorUsesCustomTheme(),
                .vstPluginPort = AudioSettings::vstPluginPort(),
                .audioExporterClippingCheckEnabled =
                    AudioSettings::audioExporterClippingCheckEnabled(),
                .audioExporterIgnoredWarningFlags =
                    AudioSettings::audioExporterIgnoredWarningFlag(),
                .audioExporterUseTemporaryFile = AudioSettings::audioExporterUseTemporaryFile(),
            };
            for (int index = 0; index < 4; ++index) {
                result.pseudoSingerSynthesizers.append({
                    .generator = AudioSettings::pseudoSingerSynthGenerator(index),
                    .amplitude = AudioSettings::pseudoSingerSynthAmplitude(index),
                    .attackMilliseconds = AudioSettings::pseudoSingerSynthAttackMsec(index),
                    .decayMilliseconds = AudioSettings::pseudoSingerSynthDecayMsec(index),
                    .decayRatio = AudioSettings::pseudoSingerSynthDecayRatio(index),
                    .releaseMilliseconds = AudioSettings::pseudoSingerSynthReleaseMsec(index),
                });
            }
            const auto presetObject =
                options->audio()->obj.value(QStringLiteral("audioExporterPresets")).toObject();
            for (auto it = presetObject.constBegin(); it != presetObject.constEnd(); ++it) {
                const auto config = it.value().toObject().toVariantMap();
                result.audioExporterPresets.append({
                    .name = it.key(),
                    .config =
                        {
                                 .fileName = config.value(QStringLiteral("fileName")).toString(),
                                 .fileDirectory =
                                config.value(QStringLiteral("fileDirectory")).toString(),
                                 .fileType = config.value(QStringLiteral("fileType")).toInt(),
                                 .mono = config.value(QStringLiteral("formatMono")).toBool(),
                                 .formatOption = config.value(QStringLiteral("formatOption")).toInt(),
                                 .formatQuality = config.value(QStringLiteral("formatQuality")).toInt(),
                                 .sampleRate =
                                config.value(QStringLiteral("formatSampleRate")).toDouble(),
                                 .mixingOption = config.value(QStringLiteral("mixingOption")).toInt(),
                                 .muteSoloEnabled =
                                config.value(QStringLiteral("isMuteSoloEnabled")).toBool(),
                                 .sourceOption = config.value(QStringLiteral("sourceOption")).toInt(),
                                 .sources = config.value(QStringLiteral("source")).value<QList<int>>(),
                                 .timeRange = config.value(QStringLiteral("timeRange")).toInt(),
                                 },
                });
            }
            const auto currentPreset =
                options->audio()->obj.value(QStringLiteral("audioExporterCurrentPreset"));
            result.currentAudioExporterPresetIsName = currentPreset.isString();
            if (result.currentAudioExporterPresetIsName)
                result.currentAudioExporterPreset = currentPreset.toString();
            else
                result.legacyAudioExporterPresetIndex = currentPreset.toInt();
            return result;
        }

        void restoreAudio(AppOptions *options, const AudioSettingsDto &value) {
            auto &object = options->audio()->obj;
            object[QStringLiteral("adoptedBufferSize")] = value.adoptedBufferSize;
            object[QStringLiteral("adoptedSampleRate")] = value.adoptedSampleRate;
            object[QStringLiteral("deviceGain")] = value.deviceGain;
            object[QStringLiteral("deviceName")] = value.deviceName;
            object[QStringLiteral("devicePan")] = value.devicePan;
            object[QStringLiteral("driverName")] = value.driverName;
            object[QStringLiteral("fileBufferingReadAheadSize")] = value.fileBufferingReadAheadSize;
            object[QStringLiteral("hotPlugNotificationMode")] = value.hotPlugNotificationMode;
            object[QStringLiteral("playheadBehavior")] = value.playheadBehavior;
            object[QStringLiteral("midiDeviceIndex")] = value.midiDeviceIndex;
            object[QStringLiteral("midiSynthesizerAmplitude")] = value.midiSynthesizerAmplitude;
            object[QStringLiteral("midiSynthesizerAttackMsec")] =
                value.midiSynthesizerAttackMilliseconds;
            object[QStringLiteral("midiSynthesizerDecayMsec")] =
                value.midiSynthesizerDecayMilliseconds;
            object[QStringLiteral("midiSynthesizerDecayRatio")] = value.midiSynthesizerDecayRatio;
            object[QStringLiteral("midiSynthesizerFrequencyOfA")] =
                value.midiSynthesizerFrequencyOfA;
            object[QStringLiteral("midiSynthesizerGenerator")] = value.midiSynthesizerGenerator;
            object[QStringLiteral("midiSynthesizerReleaseMsec")] =
                value.midiSynthesizerReleaseMilliseconds;
            object[QStringLiteral("pseudoSingerReadEnergy")] = value.pseudoSingerReadEnergy;
            object[QStringLiteral("pseudoSingerReadPitch")] = value.pseudoSingerReadPitch;
            for (int index = 0; index < value.pseudoSingerSynthesizers.size(); ++index) {
                const auto prefix = QString::number(index);
                const auto &synthesizer = value.pseudoSingerSynthesizers.at(index);
                object[prefix + QStringLiteral("pseudoSingerSynthGenerator")] =
                    synthesizer.generator;
                object[prefix + QStringLiteral("pseudoSingerSynthAmplitude")] =
                    synthesizer.amplitude;
                object[prefix + QStringLiteral("pseudoSingerSynthAttackMsec")] =
                    synthesizer.attackMilliseconds;
                object[prefix + QStringLiteral("pseudoSingerSynthDecayMsec")] =
                    synthesizer.decayMilliseconds;
                object[prefix + QStringLiteral("pseudoSingerSynthDecayRatio")] =
                    synthesizer.decayRatio;
                object[prefix + QStringLiteral("pseudoSingerSynthReleaseMsec")] =
                    synthesizer.releaseMilliseconds;
            }
            object[QStringLiteral("vstEditorPort")] = value.vstEditorPort;
            object[QStringLiteral("vstPluginEditorUsesCustomTheme")] =
                value.vstPluginEditorUsesCustomTheme;
            object[QStringLiteral("vstPluginPort")] = value.vstPluginPort;
            object[QStringLiteral("audioExporterClippingCheckEnabled")] =
                value.audioExporterClippingCheckEnabled;
            object[QStringLiteral("audioExporterIgnoredWarningFlag")] =
                value.audioExporterIgnoredWarningFlags;
            object[QStringLiteral("audioExporterUseTemporaryFile")] =
                value.audioExporterUseTemporaryFile;
            QJsonObject presets;
            for (const auto &preset : value.audioExporterPresets) {
                const auto &config = preset.config;
                presets.insert(preset.name,
                               QJsonObject::fromVariantMap({
                                   {QStringLiteral("fileName"),          config.fileName                    },
                                   {QStringLiteral("fileDirectory"),     config.fileDirectory               },
                                   {QStringLiteral("fileType"),          config.fileType                    },
                                   {QStringLiteral("formatMono"),        config.mono                        },
                                   {QStringLiteral("formatOption"),      config.formatOption                },
                                   {QStringLiteral("formatQuality"),     config.formatQuality               },
                                   {QStringLiteral("formatSampleRate"),  config.sampleRate                  },
                                   {QStringLiteral("mixingOption"),      config.mixingOption                },
                                   {QStringLiteral("isMuteSoloEnabled"), config.muteSoloEnabled             },
                                   {QStringLiteral("sourceOption"),      config.sourceOption                },
                                   {QStringLiteral("source"),            QVariant::fromValue(config.sources)},
                                   {QStringLiteral("timeRange"),         config.timeRange                   },
                }));
            }
            object[QStringLiteral("audioExporterPresets")] = presets;
            object[QStringLiteral("audioExporterCurrentPreset")] =
                value.currentAudioExporterPresetIsName
                    ? QJsonValue(value.currentAudioExporterPreset)
                    : QJsonValue(value.legacyAudioExporterPresetIndex);
        }

        void restoreWindow(AppOptions *options, const WindowSettingsDto &value) {
            options->window()->setMainWindowGeometry(value.mainWindowGeometry);
        }

        SettingsSnapshotDto captureAll(AppOptions *options) {
            return {
                .general = captureGeneral(options),
                .appearance = captureAppearance(options),
                .inference = captureInference(options),
                .developer = captureDeveloper(options),
                .g2pLanguage = captureG2pLanguage(options),
                .fillLyric = captureFillLyric(options),
                .window = captureWindow(options),
                .audio = captureAudio(options),
            };
        }

        QList<SpeakerMixPresetDto> captureSpeakerMixPresets(AppOptions *options) {
            const auto root = options->general()->speakerMixPresets.toObject();
            if (root.value(QStringLiteral("schemaVersion")).toInt(kSpeakerMixPresetSchemaVersion) !=
                kSpeakerMixPresetSchemaVersion) {
                return {};
            }
            QList<SpeakerMixPresetDto> result;
            for (const auto &item : root.value(QStringLiteral("presets")).toArray()) {
                const auto object = item.toObject();
                SpeakerMixPresetDto preset{
                    .id = object.value(QStringLiteral("id")).toString(),
                    .name = object.value(QStringLiteral("name")).toString(),
                    .packageId = object.value(QStringLiteral("packageId")).toString(),
                    .singerId = object.value(QStringLiteral("singerId")).toString(),
                    .packageVersion = QVersionNumber::fromString(
                        object.value(QStringLiteral("packageVersion")).toString()),
                    .createdAt = QDateTime::fromString(
                        object.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs),
                    .updatedAt = QDateTime::fromString(
                        object.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs),
                };
                for (const auto &sourceValue : object.value(QStringLiteral("sources")).toArray()) {
                    const auto source = sourceValue.toObject();
                    const auto speakerId = source.value(QStringLiteral("id")).toString();
                    if (!speakerId.isEmpty()) {
                        preset.sources.append({
                            .speakerId = speakerId,
                            .speakerName = source.value(QStringLiteral("name")).toString(),
                        });
                    }
                }
                for (const auto &weight : object.value(QStringLiteral("fixedWeights")).toArray())
                    preset.fixedWeights.append(weight.toDouble());
                if (!preset.id.isEmpty() && !preset.name.isEmpty() && !preset.singerId.isEmpty() &&
                    !preset.packageId.isEmpty()) {
                    result.append(std::move(preset));
                }
            }
            return result;
        }

        bool applySpeakerMixPresets(AppOptions *options,
                                    const QList<SpeakerMixPresetDto> &presets) {
            QJsonArray presetArray;
            for (const auto &preset : presets) {
                QJsonArray sources;
                for (const auto &source : preset.sources) {
                    sources.append(QJsonObject{
                        {QStringLiteral("id"),   source.speakerId  },
                        {QStringLiteral("name"), source.speakerName},
                    });
                }
                QJsonArray weights;
                for (const double weight : preset.fixedWeights)
                    weights.append(weight);
                presetArray.append(QJsonObject{
                    {QStringLiteral("id"),             preset.id                       },
                    {QStringLiteral("name"),           preset.name                     },
                    {QStringLiteral("packageId"),      preset.packageId                },
                    {QStringLiteral("singerId"),       preset.singerId                 },
                    {QStringLiteral("packageVersion"), preset.packageVersion.toString()},
                    {QStringLiteral("sources"),        sources                         },
                    {QStringLiteral("fixedWeights"),   weights                         },
                    {QStringLiteral("createdAt"),
                     preset.createdAt.toUTC().toString(Qt::ISODateWithMs)              },
                    {QStringLiteral("updatedAt"),
                     preset.updatedAt.toUTC().toString(Qt::ISODateWithMs)              },
                });
            }
            const auto previous = options->general()->speakerMixPresets;
            options->general()->speakerMixPresets = QJsonObject{
                {QStringLiteral("schemaVersion"), kSpeakerMixPresetSchemaVersion},
                {QStringLiteral("presets"),       presetArray                   },
            };
            if (options->saveAndNotify(AppOptionsGlobal::General))
                return true;
            options->general()->speakerMixPresets = previous;
            options->notifyOptionsChanged(AppOptionsGlobal::General);
            return false;
        }

        template <typename T, typename Capture, typename Restore>
        bool applyAndSave(AppOptions *options, const T &value,
                          const AppOptionsGlobal::Option category, Capture capture,
                          Restore restore) {
            const auto previous = capture(options);
            restore(options, value);
            if (options->saveAndNotify(category))
                return true;
            restore(options, previous);
            options->notifyOptionsChanged(category);
            return false;
        }
    }

    SettingsRuntimeServices createAppOptionsAutomationServices(AppOptions *options) {
        SettingsRuntimeServices services;
        if (!options)
            return services;
        services.snapshot = [options] { return captureAll(options); };
        services.applyGeneral = [options](const GeneralSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::General, captureGeneral,
                                restoreGeneral);
        };
        services.applyAppearance = [options](const AppearanceSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::Appearance, captureAppearance,
                                restoreAppearance);
        };
        services.applyInference = [options](const InferenceSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::Inference, captureInference,
                                restoreInference);
        };
        services.applyDeveloper = [options](const DeveloperSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::DeveloperOptions,
                                captureDeveloper, restoreDeveloper);
        };
        services.applyG2pLanguage = [options](const G2pLanguageSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::G2pLanguage, captureG2pLanguage,
                                restoreG2pLanguage);
        };
        services.applyFillLyric = [options](const FillLyricSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::FillLyric, captureFillLyric,
                                restoreFillLyric);
        };
        services.applyWindow = [options](const WindowSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::Window, captureWindow,
                                restoreWindow);
        };
        services.applyAudio = [options](const AudioSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::Audio, captureAudio,
                                restoreAudio);
        };
        return services;
    }

    PresetRuntimeServices createAppOptionsPresetAutomationServices(AppOptions *options) {
        PresetRuntimeServices services;
        if (!options)
            return services;
        services.speakerMixPresets = [options] { return captureSpeakerMixPresets(options); };
        services.applySpeakerMixPresets = [options](const QList<SpeakerMixPresetDto> &presets) {
            return applySpeakerMixPresets(options, presets);
        };
        return services;
    }

} // namespace Automation
