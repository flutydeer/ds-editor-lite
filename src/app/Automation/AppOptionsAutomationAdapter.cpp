#include "AppOptionsAutomationAdapter.h"

#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioSettings.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/FillLyric/Utils/LyricRuleAutomationUtils.h"
#include "Modules/FillLyric/Utils/TaggerRuleOrder.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"
#include "Utils/UiLanguageManager.h"

#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include <TalcsCore/MixerAudioSource.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDriverManager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <iterator>
#include <memory>

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
                    .ruleId = rule.ruleId,
                    .name = rule.name,
                    .regexes = rule.regexes,
                    .enabled = rule.enabled,
                    .order = rule.order,
                });
            }
            for (const auto &rule : value->customTaggerRules) {
                TaggerRuleDto converted{
                    .ruleId = rule.ruleId,
                    .name = rule.name,
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

        QList<CustomTaggerRule> customTaggerRules(const QList<TaggerRuleDto> &rules) {
            QList<CustomTaggerRule> result;
            result.reserve(rules.size());
            for (const auto &rule : rules) {
                CustomTaggerRule converted{
                    .ruleId = rule.ruleId,
                    .name = rule.name,
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
                result.append(std::move(converted));
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
                    .ruleId = rule.ruleId,
                    .name = rule.name,
                    .regexes = rule.regexes,
                    .enabled = rule.enabled,
                    .order = rule.order,
                });
            }
            target->customTaggerRules = customTaggerRules(value.customTaggerRules);
            target->ensureStableRuleIds();
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

        bool syncFillLyricRuntime(AppOptions *options) {
            const auto *value = options->fillLyric();
            if (!FillLyric::TextTagger::validateCustomRules(value->customTaggerRules).isEmpty())
                return false;
            FillLyric::TextSplitter::setBuiltinEnabled(value->builtinSplitterEnabled);
            FillLyric::TextSplitter::setCustomRules(value->customSplitterRules);
            FillLyric::TextSplitter::setRuleOrder(value->splitterOrder);
            FillLyric::TextTagger::setBuiltinEnabled(value->builtinTaggerEnabled);
            if (!FillLyric::TextTagger::setCustomRules(value->customTaggerRules))
                return false;
            FillLyric::TextTagger::setRuleOrder(value->taggerOrder);
            return true;
        }

        bool applyFillLyricAndSave(AppOptions *options, const FillLyricSettingsDto &value) {
            const auto previous = captureFillLyric(options);
            restoreFillLyric(options, value);
            if (!syncFillLyricRuntime(options)) {
                restoreFillLyric(options, previous);
                syncFillLyricRuntime(options);
                return false;
            }
            if (options->saveAndNotify(AppOptionsGlobal::FillLyric))
                return true;
            restoreFillLyric(options, previous);
            syncFillLyricRuntime(options);
            options->notifyOptionsChanged(AppOptionsGlobal::FillLyric);
            return false;
        }

        QList<LyricRuleDto> captureLyricRules(AppOptions *options) {
            const auto *value = options->fillLyric();
            QList<LyricRuleDto> splitterRules;
            for (const auto &info : FillLyric::TextSplitter::ruleInfoList()) {
                if (!info.builtin)
                    continue;
                splitterRules.append({
                    .ruleId =
                        FillLyric::builtinAutomationRuleId(QStringLiteral("splitter"), info.name),
                    .kind = LyricRuleKind::Splitter,
                    .builtin = true,
                    .name = info.name,
                    .regexes = info.regexes,
                    .enabled = value->builtinSplitterEnabled.value(info.name, true),
                    .engineOrderKey = info.name,
                });
            }
            for (const auto &rule : value->customSplitterRules) {
                splitterRules.append({
                    .ruleId = rule.ruleId,
                    .kind = LyricRuleKind::Splitter,
                    .builtin = false,
                    .name = rule.name,
                    .regexes = rule.regexes,
                    .enabled = rule.enabled,
                    .order = rule.order,
                    .engineOrderKey = rule.name,
                });
            }
            QList<LyricRuleDto> orderedSplitters;
            orderedSplitters.reserve(splitterRules.size());
            QSet<QString> consumedSplitterIds;
            for (const auto &key : value->splitterOrder) {
                const auto found = std::find_if(
                    splitterRules.cbegin(), splitterRules.cend(), [&](const auto &rule) {
                        return !consumedSplitterIds.contains(rule.ruleId) &&
                               rule.engineOrderKey == key;
                    });
                if (found != splitterRules.cend()) {
                    orderedSplitters.append(*found);
                    consumedSplitterIds.insert(found->ruleId);
                }
            }
            for (const auto &rule : std::as_const(splitterRules)) {
                if (!consumedSplitterIds.contains(rule.ruleId))
                    orderedSplitters.append(rule);
            }
            for (qsizetype index = 0; index < orderedSplitters.size(); ++index)
                orderedSplitters[index].order = static_cast<int>(index);

            QList<LyricRuleDto> taggerRules;
            QList<FillLyric::TaggerRuleIdentity> identities;
            for (const auto &info : FillLyric::TextTagger::ruleInfoList()) {
                if (!info.builtin)
                    continue;
                LyricRuleDto converted{
                    .ruleId =
                        FillLyric::builtinAutomationRuleId(QStringLiteral("tagger"), info.language),
                    .kind = LyricRuleKind::Tagger,
                    .builtin = true,
                    .name = info.language,
                    .language = info.language,
                    .enabled = value->builtinTaggerEnabled.value(info.language, true),
                    .engineOrderKey = FillLyric::TaggerRuleOrder::key(
                        {.language = info.language, .builtin = true}),
                };
                for (const auto &entry : info.entries) {
                    converted.entries.append({
                        .type = entry.type,
                        .value = entry.values,
                        .tag = entry.tag,
                        .discard = entry.discard,
                    });
                }
                taggerRules.append(std::move(converted));
                identities.append({.language = info.language, .builtin = true});
            }
            for (const auto &rule : value->customTaggerRules) {
                LyricRuleDto converted{
                    .ruleId = rule.ruleId,
                    .kind = LyricRuleKind::Tagger,
                    .builtin = false,
                    .name = rule.name,
                    .language = rule.language,
                    .enabled = rule.enabled,
                    .engineOrderKey = FillLyric::TaggerRuleOrder::key(
                        {.language = rule.language, .builtin = false}),
                };
                for (const auto &entry : rule.entries) {
                    converted.entries.append({
                        .type = entry.type,
                        .value = entry.value,
                        .tag = entry.tag,
                        .discard = entry.discard,
                    });
                }
                taggerRules.append(std::move(converted));
                identities.append({.language = rule.language, .builtin = false});
            }
            QList<LyricRuleDto> orderedTaggers;
            orderedTaggers.reserve(taggerRules.size());
            for (const int index :
                 FillLyric::TaggerRuleOrder::resolve(value->taggerOrder, identities))
                orderedTaggers.append(taggerRules.at(index));
            for (qsizetype index = 0; index < orderedTaggers.size(); ++index)
                orderedTaggers[index].order = static_cast<int>(index);

            orderedSplitters.append(orderedTaggers);
            return orderedSplitters;
        }

        AutomationResult<LyricRuleTestResultDto> testLyricRulesRuntime(const QString &text) {
            const auto split = FillLyric::TextSplitter::split(text.toStdString());
            LyricRuleTestResultDto result;
            result.splitTokens.reserve(static_cast<qsizetype>(split.size()));
            for (const auto &token : split)
                result.splitTokens.append(QString::fromStdString(token));
            const auto tagged = FillLyric::TextTagger::tag(split, false);
            result.taggedTokens.reserve(static_cast<qsizetype>(tagged.size()));
            for (const auto &token : tagged) {
                result.taggedTokens.append({
                    .lyric = QString::fromStdString(token.lyric),
                    .language = QString::fromStdString(token.language),
                    .tag = QString::fromStdString(token.tag),
                    .discard = token.discard,
                });
            }
            return result;
        }

        SettingsStringCandidateDto settingsCandidate(const QString &id, const bool available = true,
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

        bool audioRuntimeAvailable();

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

        AudioDevicePublicValueDto effectiveAudioValue(const AudioSettingsDto &fallback) {
            if (!AudioSystem::instance())
                return publicAudioValue(fallback);
            auto *output = AudioSystem::outputSystem();
            auto *context = output ? output->outputContext() : nullptr;
            if (!context || !context->controlMixer())
                return publicAudioValue(fallback);
            auto result = publicAudioValue(fallback);
            if (context->driver())
                result.driverName = context->driver()->name();
            if (context->device())
                result.deviceName = context->device()->name();
            result.bufferSize = context->adoptedBufferSize();
            result.sampleRate = context->adoptedSampleRate();
            result.hotPlugNotificationMode = static_cast<int>(context->hotPlugNotificationMode());
            result.gain = context->controlMixer()->gain();
            result.pan = context->controlMixer()->pan();
            return result;
        }

        QList<AudioDriverCandidateDto> audioDriverCandidates() {
            QList<AudioDriverCandidateDto> result;
            if (!AudioSystem::instance())
                return result;
            auto *output = AudioSystem::outputSystem();
            auto *context = output ? output->outputContext() : nullptr;
            if (!context || !context->driverManager())
                return result;
            const auto currentDriverName =
                context->driver() ? context->driver()->name() : QString{};
            for (const auto &driverName : context->driverManager()->drivers()) {
                AudioDriverCandidateDto driver{
                    .id = driverName,
                    .displayName = driverName,
                };
                auto *candidateDriver = context->driverManager()->driver(driverName);
                if (!candidateDriver) {
                    driver.available = false;
                    driver.unavailableReason = QStringLiteral("Audio driver is unavailable");
                    result.append(std::move(driver));
                    continue;
                }
                if (!candidateDriver->defaultDevice().isEmpty()) {
                    driver.devices.append({
                        .id = {},
                        .displayName = QStringLiteral("Default device"),
                    });
                }
                for (const auto &deviceName : candidateDriver->devices()) {
                    driver.devices.append({
                        .id = deviceName,
                        .displayName = deviceName,
                    });
                }
                if (context->driver() && driverName == currentDriverName && context->device()) {
                    const auto currentDeviceName = context->device()->name();
                    auto currentDevice = std::find_if(
                        driver.devices.begin(), driver.devices.end(),
                        [&](const auto &candidate) { return candidate.id == currentDeviceName; });
                    if (currentDevice == driver.devices.end()) {
                        driver.devices.append({
                            .id = currentDeviceName,
                            .displayName = currentDeviceName,
                        });
                        currentDevice = std::prev(driver.devices.end());
                    }
                    currentDevice->bufferSizes = context->device()->availableBufferSizes();
                    currentDevice->sampleRates = context->device()->availableSampleRates();
                }
                result.append(std::move(driver));
            }
            return result;
        }

        PublicSettingsSnapshotDto
            capturePublicSettings(AppOptions *options,
                                  const SettingsSnapshotDto &effectiveSettings) {
            const auto configured = captureAll(options);
            PublicSettingsSnapshotDto result;
            const auto *languageManager = UiLanguageManager::instance();
            result.uiLanguage = UiLanguagePublicSettingsDto{
                .configured = configured.general.uiLanguage,
                .effective = languageManager ? languageManager->effectiveLanguageId()
                                             : effectiveSettings.general.uiLanguage,
                .candidates = {settingsCandidate(UiLanguageManager::System),
                               settingsCandidate(UiLanguageManager::English),
                               settingsCandidate(UiLanguageManager::SimplifiedChinese)},
            };
            QList<SettingsStringCandidateDto> languageCandidates;
            auto languages = configured.general.defaultLyrics.keys();
            if (!languages.contains(configured.general.defaultSingingLanguage))
                languages.append(configured.general.defaultSingingLanguage);
            for (const auto &language : std::as_const(languages))
                languageCandidates.append(settingsCandidate(language));
            result.singing = SingingPublicSettingsDto{
                .configuredDefaultLanguage = configured.general.defaultSingingLanguage,
                .effectiveDefaultLanguage = effectiveSettings.general.defaultSingingLanguage,
                .configuredDefaultLyrics = configured.general.defaultLyrics,
                .effectiveDefaultLyrics = effectiveSettings.general.defaultLyrics,
                .languageCandidates = std::move(languageCandidates),
            };
            auto effectiveTheme = effectiveSettings.appearance.themeId;
            if (auto *themeManager = ThemeManager::instance();
                !themeManager->currentThemeId().isEmpty())
                effectiveTheme = themeManager->currentThemeId();
            result.theme = ThemePublicSettingsDto{
                .configured = configured.appearance.themeId,
                .effective = effectiveTheme,
                .candidates = {settingsCandidate(ThemeIds::systemThemePreferenceId()),
                               settingsCandidate(ThemeIds::lightThemePreferenceId()),
                               settingsCandidate(ThemeIds::darkThemePreferenceId())},
            };
            result.audioDevice = AudioDevicePublicSettingsDto{
                .configured = publicAudioValue(configured.audio),
                .effective = effectiveAudioValue(effectiveSettings.audio),
                .drivers = audioDriverCandidates(),
                .unavailableReason = audioRuntimeAvailable()
                                         ? QString{}
                                         : QStringLiteral("Audio output system is unavailable"),
            };
            result.playbackBehavior = PlaybackBehaviorPublicSettingsDto{
                .configured = configured.audio.playheadBehavior,
                .effective = effectiveSettings.audio.playheadBehavior,
                .candidates = {0, 1, 2},
            };
            QList<SettingsStringCandidateDto> providerCandidates{
                settingsCandidate(QStringLiteral("CPU")),
                settingsCandidate(QStringLiteral("DirectML")),
            };
#ifdef ONNXRUNTIME_ENABLE_CUDA
            providerCandidates.append(settingsCandidate(QStringLiteral("CUDA")));
#else
            providerCandidates.append(settingsCandidate(
                QStringLiteral("CUDA"), false,
                QStringLiteral("This build does not include the CUDA execution provider")));
#endif
            QList<SettingsGpuCandidateDto> gpuCandidates;
            if (!configured.inference.selectedGpuId.isEmpty()) {
                gpuCandidates.append({
                    .index = configured.inference.selectedGpuIndex,
                    .id = configured.inference.selectedGpuId,
                    .displayName = configured.inference.selectedGpuId,
                });
            }
            result.computeDevice = ComputeDevicePublicSettingsDto{
                .configured = publicComputeValue(configured.inference),
                .effective = publicComputeValue(effectiveSettings.inference),
                .providerCandidates = std::move(providerCandidates),
                .gpuCandidates = std::move(gpuCandidates),
            };
            if (configured.inference.executionProvider !=
                effectiveSettings.inference.executionProvider) {
                result.computeDevice->restartRequiredFields.append(
                    QStringLiteral("execution_provider"));
            }
            if (configured.inference.selectedGpuIndex !=
                effectiveSettings.inference.selectedGpuIndex) {
                result.computeDevice->restartRequiredFields.append(QStringLiteral("gpu_index"));
            }
            if (configured.inference.selectedGpuId != effectiveSettings.inference.selectedGpuId)
                result.computeDevice->restartRequiredFields.append(QStringLiteral("gpu_id"));
            result.render = RenderPublicSettingsDto{
                .configured = publicRenderValue(configured.inference),
                .effective = publicRenderValue(effectiveSettings.inference),
                .samplingStepsRange = {1.0, 1000.0, 1.0 },
                .depthRange = {0.0, 1.0,    0.01},
                .playbackLookaheadRange = {1.0, 60.0,   1.0 },
                .pitchSmoothKernelRange = {0.0, 50.0,   1.0 },
            };
            if (configured.inference.runVocoderOnCpu !=
                effectiveSettings.inference.runVocoderOnCpu) {
                result.render->restartRequiredFields.append(QStringLiteral("run_vocoder_on_cpu"));
            }
            result.singerSessionRetention = SingerSessionRetentionPublicSettingsDto{
                .configured = publicRetentionValue(configured.inference),
                .effective = publicRetentionValue(effectiveSettings.inference),
                .capacityCandidates = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                .idleTimeoutCandidates = {0, 60, 120, 180, 240, 300},
            };
            result.packageSearchPaths = PackageSearchPathsPublicSettingsDto{
                .configured = configured.general.packageSearchPaths,
                .effective = effectiveSettings.general.packageSearchPaths,
                .restartRequired = configured.general.packageSearchPaths !=
                                   effectiveSettings.general.packageSearchPaths,
            };
            return result;
        }

        AutomationError runtimeApplyError(const QString &field, const QString &message) {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.fieldPath = field;
            error.message = message;
            return error;
        }

        struct AudioRuntimeState {
            QString driverName;
            QString deviceName;
            qint64 bufferSize = 0;
            double sampleRate = 0.0;
            int hotPlugNotificationMode = 0;
            double gain = 1.0;
            double pan = 0.0;
        };

        bool audioRuntimeAvailable() {
            if (!AudioSystem::instance())
                return false;
            auto *output = AudioSystem::outputSystem();
            auto *context = output ? output->outputContext() : nullptr;
            return context && context->driverManager() && context->controlMixer();
        }

        AudioRuntimeState captureAudioRuntimeState(OutputSystem *output) {
            const auto *context = output->outputContext();
            return {
                .driverName = context->driver() ? context->driver()->name() : QString{},
                .deviceName = context->device() ? context->device()->name() : QString{},
                .bufferSize = context->adoptedBufferSize(),
                .sampleRate = context->adoptedSampleRate(),
                .hotPlugNotificationMode = static_cast<int>(context->hotPlugNotificationMode()),
                .gain = context->controlMixer()->gain(),
                .pan = context->controlMixer()->pan(),
            };
        }

        bool applyAudioRuntimeState(OutputSystem *output, const AudioRuntimeState &state) {
            if (!output || !output->outputContext() || !output->outputContext()->controlMixer())
                return false;
            auto *context = output->outputContext();
            if ((!context->driver() || context->driver()->name() != state.driverName) &&
                !context->setDriver(state.driverName)) {
                return false;
            }
            if ((!context->device() || context->device()->name() != state.deviceName) &&
                !context->setDevice(state.deviceName)) {
                return false;
            }
            if (context->adoptedBufferSize() != state.bufferSize &&
                !context->setAdoptedBufferSize(state.bufferSize)) {
                return false;
            }
            if (context->adoptedSampleRate() != state.sampleRate &&
                !context->setAdoptedSampleRate(state.sampleRate)) {
                return false;
            }
            context->setHotPlugNotificationMode(
                static_cast<talcs::OutputContext::HotPlugNotificationMode>(
                    state.hotPlugNotificationMode));
            context->controlMixer()->setGain(static_cast<float>(state.gain));
            context->controlMixer()->setPan(static_cast<float>(state.pan));
            return true;
        }

        bool persistAudioSettings(AppOptions *options, const AudioSettingsDto &value) {
            const auto previous = captureAudio(options);
            restoreAudio(options, value);
            if (options->saveAndNotify(AppOptionsGlobal::Audio))
                return true;
            restoreAudio(options, previous);
            options->notifyOptionsChanged(AppOptionsGlobal::Audio);
            return false;
        }

        AutomationResult<AutomationUnit>
            applyAudioDeviceAndSave(AppOptions *options, const AudioSettingsDto &value,
                                    const AudioDeviceSettingsPatchDto &patch) {
            if (!audioRuntimeAvailable()) {
                return runtimeApplyError(QStringLiteral("audio_device"),
                                         QStringLiteral("Audio output system is unavailable"));
            }
            auto *output = AudioSystem::outputSystem();
            const auto previousRuntime = captureAudioRuntimeState(output);
            auto *context = output->outputContext();
            bool applied = true;
            if (patch.driverName &&
                (!context->driver() || context->driver()->name() != value.driverName)) {
                applied = context->setDriver(value.driverName);
            }
            if (applied && patch.deviceName &&
                (!context->device() || context->device()->name() != value.deviceName)) {
                applied = context->setDevice(value.deviceName);
            }
            if (applied && patch.bufferSize &&
                context->adoptedBufferSize() != value.adoptedBufferSize) {
                applied = context->setAdoptedBufferSize(value.adoptedBufferSize);
            }
            if (applied && patch.sampleRate &&
                context->adoptedSampleRate() != value.adoptedSampleRate) {
                applied = context->setAdoptedSampleRate(value.adoptedSampleRate);
            }
            if (!applied) {
                applyAudioRuntimeState(output, previousRuntime);
                return runtimeApplyError(
                    QStringLiteral("audio_device"),
                    QStringLiteral("Audio device settings could not be applied"));
            }
            if (patch.hotPlugNotificationMode) {
                context->setHotPlugNotificationMode(
                    static_cast<talcs::OutputContext::HotPlugNotificationMode>(
                        value.hotPlugNotificationMode));
            }
            if (patch.gain)
                context->controlMixer()->setGain(static_cast<float>(value.deviceGain));
            if (patch.pan)
                context->controlMixer()->setPan(static_cast<float>(value.devicePan));

            auto persistedValue = value;
            const auto effective = captureAudioRuntimeState(output);
            const auto deviceSelectionChanged = patch.driverName || patch.deviceName;
            if (deviceSelectionChanged) {
                persistedValue.driverName = effective.driverName;
                persistedValue.deviceName = effective.deviceName;
                persistedValue.adoptedBufferSize = effective.bufferSize;
                persistedValue.adoptedSampleRate = effective.sampleRate;
            } else {
                if (patch.bufferSize)
                    persistedValue.adoptedBufferSize = effective.bufferSize;
                if (patch.sampleRate)
                    persistedValue.adoptedSampleRate = effective.sampleRate;
            }
            if (patch.hotPlugNotificationMode)
                persistedValue.hotPlugNotificationMode = effective.hotPlugNotificationMode;
            if (patch.gain)
                persistedValue.deviceGain = effective.gain;
            if (patch.pan)
                persistedValue.devicePan = effective.pan;
            if (!persistAudioSettings(options, persistedValue)) {
                applyAudioRuntimeState(output, previousRuntime);
                return runtimeApplyError(QStringLiteral("audio_device"),
                                         QStringLiteral("Audio settings could not be saved"));
            }
            return AutomationUnit{};
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
        const auto effectiveSettings = std::make_shared<SettingsSnapshotDto>(captureAll(options));
        QObject::connect(
            options, &AppOptions::optionsChanged, options,
            [options, effectiveSettings](const AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::General) {
                    const auto effectivePackagePaths =
                        effectiveSettings->general.packageSearchPaths;
                    effectiveSettings->general = captureGeneral(options);
                    effectiveSettings->general.packageSearchPaths = effectivePackagePaths;
                }
                if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::Appearance) {
                    effectiveSettings->appearance = captureAppearance(options);
                }
                if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::Audio)
                    effectiveSettings->audio = captureAudio(options);
                if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::Inference) {
                    const auto provider = effectiveSettings->inference.executionProvider;
                    const auto gpuIndex = effectiveSettings->inference.selectedGpuIndex;
                    const auto gpuId = effectiveSettings->inference.selectedGpuId;
                    const auto vocoderOnCpu = effectiveSettings->inference.runVocoderOnCpu;
                    effectiveSettings->inference = captureInference(options);
                    effectiveSettings->inference.executionProvider = provider;
                    effectiveSettings->inference.selectedGpuIndex = gpuIndex;
                    effectiveSettings->inference.selectedGpuId = gpuId;
                    effectiveSettings->inference.runVocoderOnCpu = vocoderOnCpu;
                }
            });
        services.snapshot = [options] { return captureAll(options); };
        services.publicSnapshot = [options, effectiveSettings] {
            return capturePublicSettings(options, *effectiveSettings);
        };
        services.applyGeneral = [options, effectiveSettings](const GeneralSettingsDto &value) {
            if (!applyAndSave(options, value, AppOptionsGlobal::General, captureGeneral,
                              restoreGeneral)) {
                return false;
            }
            const auto effectivePackagePaths = effectiveSettings->general.packageSearchPaths;
            effectiveSettings->general = value;
            effectiveSettings->general.packageSearchPaths = effectivePackagePaths;
            return true;
        };
        services.applyAppearance = [options,
                                    effectiveSettings](const AppearanceSettingsDto &value) {
            if (!applyAndSave(options, value, AppOptionsGlobal::Appearance, captureAppearance,
                              restoreAppearance)) {
                return false;
            }
            effectiveSettings->appearance = value;
            return true;
        };
        services.applyInference = [options, effectiveSettings](const InferenceSettingsDto &value) {
            if (!applyAndSave(options, value, AppOptionsGlobal::Inference, captureInference,
                              restoreInference)) {
                return false;
            }
            const auto provider = effectiveSettings->inference.executionProvider;
            const auto gpuIndex = effectiveSettings->inference.selectedGpuIndex;
            const auto gpuId = effectiveSettings->inference.selectedGpuId;
            const auto vocoderOnCpu = effectiveSettings->inference.runVocoderOnCpu;
            effectiveSettings->inference = value;
            effectiveSettings->inference.executionProvider = provider;
            effectiveSettings->inference.selectedGpuIndex = gpuIndex;
            effectiveSettings->inference.selectedGpuId = gpuId;
            effectiveSettings->inference.runVocoderOnCpu = vocoderOnCpu;
            return true;
        };
        services.applyDeveloper = [options](const DeveloperSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::DeveloperOptions,
                                captureDeveloper, restoreDeveloper);
        };
        services.applyG2pLanguage = [options](const G2pLanguageSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::G2pLanguage, captureG2pLanguage,
                                restoreG2pLanguage);
        };
        services.applyFillLyric = [options, effectiveSettings](const FillLyricSettingsDto &value) {
            if (!applyFillLyricAndSave(options, value)) {
                return false;
            }
            effectiveSettings->fillLyric = captureFillLyric(options);
            return true;
        };
        services.validateFillLyricRuntime = [](const FillLyricSettingsDto &value) {
            const auto message = FillLyric::TextTagger::validateCustomRules(
                customTaggerRules(value.customTaggerRules));
            if (message.isEmpty())
                return AutomationResult<AutomationUnit>(AutomationUnit{});
            return AutomationResult<AutomationUnit>(
                AutomationError::invalidArgument(QStringLiteral("entries.value"), message));
        };
        services.applyWindow = [options](const WindowSettingsDto &value) {
            return applyAndSave(options, value, AppOptionsGlobal::Window, captureWindow,
                                restoreWindow);
        };
        services.applyAudio = [options, effectiveSettings](const AudioSettingsDto &value) {
            if (!applyAndSave(options, value, AppOptionsGlobal::Audio, captureAudio, restoreAudio))
                return false;
            effectiveSettings->audio = value;
            return true;
        };
        services.applyUiLanguage = [options, effectiveSettings](const GeneralSettingsDto &value) {
            if (!applyAndSave(options, value, AppOptionsGlobal::General, captureGeneral,
                              restoreGeneral)) {
                return AutomationResult<AutomationUnit>(
                    runtimeApplyError(QStringLiteral("ui_language"),
                                      QStringLiteral("UI language preference could not be saved")));
            }
            if (auto *manager = UiLanguageManager::instance())
                manager->setPreference(value.uiLanguage);
            const auto effectivePackagePaths = effectiveSettings->general.packageSearchPaths;
            effectiveSettings->general = value;
            effectiveSettings->general.packageSearchPaths = effectivePackagePaths;
            return AutomationResult<AutomationUnit>(AutomationUnit{});
        };
        services.applyTheme = [options, effectiveSettings](const AppearanceSettingsDto &value) {
            const auto previous = captureAppearance(options);
            auto *manager = ThemeManager::instance();
            if (manager && value.themeId != previous.themeId &&
                !manager->applyThemePreference(value.themeId)) {
                return AutomationResult<AutomationUnit>(runtimeApplyError(
                    QStringLiteral("theme_id"), QStringLiteral("Theme could not be applied")));
            }
            if (!applyAndSave(options, value, AppOptionsGlobal::Appearance, captureAppearance,
                              restoreAppearance)) {
                if (manager && value.themeId != previous.themeId)
                    manager->applyThemePreference(previous.themeId);
                return AutomationResult<AutomationUnit>(
                    runtimeApplyError(QStringLiteral("theme_id"),
                                      QStringLiteral("Theme preference could not be saved")));
            }
            effectiveSettings->appearance = value;
            return AutomationResult<AutomationUnit>(AutomationUnit{});
        };
        services.applyAudioDevice = [options,
                                     effectiveSettings](const AudioSettingsDto &value,
                                                        const AudioDeviceSettingsPatchDto &patch) {
            auto result = applyAudioDeviceAndSave(options, value, patch);
            if (result)
                effectiveSettings->audio = captureAudio(options);
            return result;
        };
        services.lyricRules = [options] { return captureLyricRules(options); };
        services.testLyricRules = testLyricRulesRuntime;
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
