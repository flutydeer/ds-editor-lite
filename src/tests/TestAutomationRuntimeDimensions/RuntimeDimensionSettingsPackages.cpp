#include "RuntimeDimensionSupport.h"

#include <cmath>
#include <limits>

namespace RuntimeDimensions {
    namespace {
        using ApplicationResult =
            Automation::AutomationResult<Automation::ApplicationMutationResult>;

        template <typename T, typename Getter, typename Invoke, typename Mutate>
        void runSettingCommand(ScenarioLog &log, const Automation::OperationId &operationId,
                               Getter getter, Invoke invoke, Mutate mutate,
                               const std::function<void(T &)> &invalidate = {}) {
            log.run(operationId, QStringLiteral("NORMAL-UNICODE"), [&] {
                RuntimeHarness harness;
                const auto originalVersion = harness.core().documentVersion();
                auto target = getter(harness);
                mutate(target);
                const auto result = invoke(harness, applicationContext(), target);
                log.expect(
                    result && result.get().changed && !result.get().validatedOnly &&
                        getter(harness) == target && harness.settingsWriteAttempts == 1 &&
                        harness.settingsWrites == 1 &&
                        harness.core().documentVersion() == originalVersion,
                    QStringLiteral("settings update must persist once without document revision"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                const auto current = getter(harness);
                const auto result = invoke(harness, applicationContext(), current);
                log.expect(result && !result.get().changed && harness.settingsWriteAttempts == 0 &&
                               harness.settingsWrites == 0,
                           QStringLiteral("identical settings update must not persist"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto original = getter(harness);
                auto target = original;
                mutate(target);
                const auto result = invoke(harness, applicationContext(true), target);
                log.expect(result && result.get().changed && result.get().validatedOnly &&
                               getter(harness) == original && harness.settingsWriteAttempts == 0,
                           QStringLiteral("settings preview must validate without persistence"));
            });
            if (invalidate) {
                log.run(operationId, QStringLiteral("INVALID-BOUNDARY"), [&] {
                    RuntimeHarness harness;
                    const auto original = getter(harness);
                    auto invalid = original;
                    invalidate(invalid);
                    const auto result = invoke(harness, applicationContext(), invalid);
                    log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                    operationId,
                                    QStringLiteral("invalid settings must retain operation ID"));
                    log.expect(getter(harness) == original && harness.settingsWriteAttempts == 0,
                               QStringLiteral("invalid settings must not reach persistence"));
                });
            }
            log.run(operationId, QStringLiteral("PERSISTENCE-FAILURE"), [&] {
                RuntimeHarness harness;
                const auto original = getter(harness);
                auto target = original;
                mutate(target);
                harness.settingsApplySucceeds = false;
                const auto result = invoke(harness, applicationContext(), target);
                log.expectError(result, Automation::AutomationErrorCode::IoError, operationId,
                                QStringLiteral("settings persistence failure must be stable"));
                log.expect(getter(harness) == original && harness.settingsWriteAttempts == 1 &&
                               harness.settingsWrites == 0,
                           QStringLiteral("failed persistence must preserve settings snapshot"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                auto target = getter(harness);
                mutate(target);
                const auto result = invoke(harness, applicationContext(), target);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing settings callback must be explicit"));
                log.expect(harness.settingsWriteAttempts == 0,
                           QStringLiteral("missing settings callback must not write"));
            });
            log.run(operationId, QStringLiteral("REPEATED-REQUEST-POLICY"), [&] {
                RuntimeHarness harness;
                auto target = getter(harness);
                mutate(target);
                const auto first = invoke(harness, applicationContext(), target);
                const auto second = invoke(harness, applicationContext(), target);
                log.expect(first && first.get().changed && second && !second.get().changed &&
                               harness.settingsWriteAttempts == 1 && harness.settingsWrites == 1,
                           QStringLiteral("unkeyed repeated settings request must persist once"));
            });
        }

        void runSettingsQuery(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::settings::get);
            log.run(operationId, QStringLiteral("ALL-CATEGORIES"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().getSettings();
                log.expect(result && result.get() == harness.settings &&
                               result.get().audio.pseudoSingerSynthesizers.size() == 4,
                           QStringLiteral("settings query must return all eight typed categories"));
            });
            log.run(operationId, QStringLiteral("UNICODE"), [&] {
                RuntimeHarness harness;
                harness.settings.general.gameDirectory = QStringLiteral("虚拟目录/游戏数据");
                harness.settings.appearance.uiFontFamily = QStringLiteral("思源黑体");
                const auto result = harness.core().settings().getSettings();
                log.expect(result &&
                               result.get().general.gameDirectory ==
                                   QStringLiteral("虚拟目录/游戏数据") &&
                               result.get().appearance.uiFontFamily == QStringLiteral("思源黑体"),
                           QStringLiteral("settings query must preserve Unicode values"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().getSettings();
                const auto captured = result ? result.get() : Automation::SettingsSnapshotDto{};
                harness.settings.general.gameDirectory = QStringLiteral("changed");
                log.expect(result && result.get() == captured && result.get() != harness.settings,
                           QStringLiteral("settings query must return a detached value snapshot"));
            });
            log.run(operationId, QStringLiteral("DETERMINISTIC"), [&] {
                RuntimeHarness harness;
                const auto first = harness.core().settings().getSettings();
                const auto second = harness.core().settings().getSettings();
                log.expect(first && second && first.get() == second.get() &&
                               harness.hostCalls.value(QStringLiteral("settings.snapshot")) == 2,
                           QStringLiteral("repeated settings query must be deterministic"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().settings().getSettings();
                log.expect(
                    result && harness.core().documentVersion() == version &&
                        harness.settingsWriteAttempts == 0 && harness.presetWrites == 0,
                    QStringLiteral("settings query must not mutate document or persistence"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().settings().getSettings();
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing settings snapshot must be decorated"));
            });
        }

        void runSettingsCommands(ScenarioLog &log) {
            runSettingCommand<Automation::GeneralSettingsDto>(
                log, Automation::OperationIds::settings::update_general,
                [](const RuntimeHarness &harness) { return harness.settings.general; },
                [](RuntimeHarness &harness, const auto &context, const auto &value) {
                    return harness.core().settings().updateGeneral(context, value);
                },
                [](auto &value) {
                    value.uiLanguage = QStringLiteral("zh_CN");
                    value.defaultSingingLanguage = QStringLiteral("jpn");
                    value.defaultLyrics.insert(QStringLiteral("jpn"), QStringLiteral("ら"));
                    value.gameDirectory = QStringLiteral("虚拟目录/游戏数据");
                },
                [](auto &value) { value.uiLanguage = QStringLiteral("unsupported"); });

            runSettingCommand<Automation::AppearanceSettingsDto>(
                log, Automation::OperationIds::settings::update_appearance,
                [](const RuntimeHarness &harness) { return harness.settings.appearance; },
                [](RuntimeHarness &harness, const auto &context, const auto &value) {
                    return harness.core().settings().updateAppearance(context, value);
                },
                [](auto &value) {
                    value.animationTimeScale = 0.5;
                    value.themeId = QStringLiteral("深色-测试");
                    value.uiFontFamily = QStringLiteral("思源黑体");
                },
                [](auto &value) {
                    value.animationTimeScale = std::numeric_limits<double>::quiet_NaN();
                });

            runSettingCommand<Automation::InferenceSettingsDto>(
                log, Automation::OperationIds::settings::update_inference,
                [](const RuntimeHarness &harness) { return harness.settings.inference; },
                [](RuntimeHarness &harness, const auto &context, const auto &value) {
                    return harness.core().settings().updateInference(context, value);
                },
                [](auto &value) {
                    value.executionProvider = QStringLiteral("DirectML");
                    value.selectedGpuIndex = 0;
                    value.selectedGpuId = QStringLiteral("GPU-测试");
                    value.samplingSteps = 1000;
                    value.cacheDirectory = QStringLiteral("缓存/推理");
                },
                [](auto &value) { value.executionProvider = QStringLiteral("UnknownProvider"); });

            runSettingCommand<Automation::DeveloperSettingsDto>(
                log, Automation::OperationIds::settings::update_developer,
                [](const RuntimeHarness &harness) { return harness.settings.developer; },
                [](RuntimeHarness &harness, const auto &context, const auto &value) {
                    return harness.core().settings().updateDeveloper(context, value);
                },
                [](auto &value) {
                    value.enableDiagnostics = true;
                    value.editorRenderBackend = Automation::EditorRenderBackend::RhiExperimental;
                },
                [](auto &value) {
                    value.editorRenderBackend = static_cast<Automation::EditorRenderBackend>(99);
                });

            runSettingCommand<Automation::G2pLanguageSettingsDto>(
                log, Automation::OperationIds::settings::update_g2p_language,
                [](const RuntimeHarness &harness) { return harness.settings.g2pLanguage; },
                [](RuntimeHarness &harness, const auto &context, const auto &value) {
                    return harness.core().settings().updateG2pLanguage(context, value);
                },
                [](auto &value) {
                    value.languageOrder = {QStringLiteral("cmn"), QStringLiteral("jpn"),
                                           QStringLiteral("eng")};
                },
                [](auto &value) {
                    value.languageOrder = {QStringLiteral("cmn"), QStringLiteral("cmn")};
                });

            runSettingCommand<Automation::FillLyricSettingsDto>(
                log, Automation::OperationIds::settings::update_fill_lyric,
                [](const RuntimeHarness &harness) { return harness.settings.fillLyric; },
                [](RuntimeHarness &harness, const auto &context, const auto &value) {
                    return harness.core().settings().updateFillLyric(context, value);
                },
                [](auto &value) {
                    value.skipSlur = true;
                    value.textEditFontSize = 16.0;
                    value.customSplitterRules.append({
                        .name = QStringLiteral("中文分词"),
                        .regexes = {QStringLiteral("[,，]")},
                    });
                    value.customTaggerRules.append({
                        .language = QStringLiteral("cmn"),
                        .entries = {{
                            .type = QStringLiteral("regex"),
                            .value = {QStringLiteral("^啦$")},
                            .tag = QStringLiteral("中文标签"),
                        }},
                    });
                },
                [](auto &value) {
                    value.textEditFontSize = std::numeric_limits<double>::infinity();
                });

            runSettingCommand<Automation::WindowSettingsDto>(
                log, Automation::OperationIds::settings::update_window,
                [](const RuntimeHarness &harness) { return harness.settings.window; },
                [](RuntimeHarness &harness, const auto &context, const auto &value) {
                    return harness.core().settings().updateWindow(context, value);
                },
                [](auto &value) { value.mainWindowGeometry = QByteArrayLiteral("geometry-测试"); });

            runSettingCommand<Automation::AudioSettingsDto>(
                log, Automation::OperationIds::settings::update_audio,
                [](const RuntimeHarness &harness) { return harness.settings.audio; },
                [](RuntimeHarness &harness, const auto &context, const auto &value) {
                    return harness.core().settings().updateAudio(context, value);
                },
                [](auto &value) {
                    value.deviceGain = 0.8;
                    value.devicePan = -1.0;
                    value.deviceName = QStringLiteral("音频设备-测试");
                    value.vstEditorPort = 65535;
                },
                [](auto &value) { value.devicePan = 2.0; });
        }

        void runRecentList(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::recent_files::list);
            log.run(operationId, QStringLiteral("EMPTY"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().getRecentProjectFiles();
                log.expect(result && result.get().isEmpty(),
                           QStringLiteral("recent list must preserve empty state"));
            });
            log.run(operationId, QStringLiteral("ORDERED-UNICODE"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {
                    QStringLiteral("projects/新歌.dspx"), QStringLiteral("projects/旧歌.dspx")};
                const auto result = harness.core().settings().getRecentProjectFiles();
                log.expect(result && result.get() == harness.settings.general.recentProjectFiles,
                           QStringLiteral("recent list must preserve Unicode order"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("a.dspx")};
                const auto result = harness.core().settings().getRecentProjectFiles();
                harness.settings.general.recentProjectFiles.clear();
                log.expect(result && result.get() == QStringList{QStringLiteral("a.dspx")},
                           QStringLiteral("recent list must be detached from settings host"));
            });
            log.run(operationId, QStringLiteral("DETERMINISTIC"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("a.dspx")};
                const auto first = harness.core().settings().getRecentProjectFiles();
                const auto second = harness.core().settings().getRecentProjectFiles();
                log.expect(first && second && first.get() == second.get(),
                           QStringLiteral("recent list query must be deterministic"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().settings().getRecentProjectFiles();
                log.expect(result && harness.settingsWriteAttempts == 0 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("recent list query must not persist or revise"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().settings().getRecentProjectFiles();
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing recent list host must be decorated"));
            });
        }

        void runRecentAdd(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::recent_files::add);
            log.run(operationId, QStringLiteral("NORMALIZE-UNICODE"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().addRecentProjectFile(
                    applicationContext(), QStringLiteral(" projects/../projects/歌曲.dspx "));
                log.expect(result && result.get().changed &&
                               harness.settings.general.recentProjectFiles ==
                                   QStringList{QStringLiteral("projects/歌曲.dspx")} &&
                               harness.settingsWrites == 1,
                           QStringLiteral("recent add must trim, clean and preserve Unicode"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().addRecentProjectFile(
                    applicationContext(true), QStringLiteral("projects/歌曲.dspx"));
                log.expect(result && result.get().validatedOnly && result.get().changed &&
                               harness.settings.general.recentProjectFiles.isEmpty() &&
                               harness.settingsWriteAttempts == 0,
                           QStringLiteral("recent add preview must not persist"));
            });
            log.run(operationId, QStringLiteral("DUPLICATE-NO-OP"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {
                    QStringLiteral("projects/歌曲.dspx")};
                const auto result = harness.core().settings().addRecentProjectFile(
                    applicationContext(), QStringLiteral("projects/./歌曲.dspx"));
                log.expect(result && !result.get().changed && harness.settingsWriteAttempts == 0,
                           QStringLiteral("normalized duplicate recent file must be a no-op"));
            });
            log.run(operationId, QStringLiteral("MAXIMUM-ORDER"), [&] {
                RuntimeHarness harness;
                for (int index = 0; index < 12; ++index) {
                    harness.core().settings().addRecentProjectFile(
                        applicationContext(), QStringLiteral("songs/%1.dspx").arg(index));
                }
                const auto result = harness.core().settings().getRecentProjectFiles();
                log.expect(result && result.get().size() == 10 &&
                               result.get().first() == QStringLiteral("songs/11.dspx") &&
                               result.get().last() == QStringLiteral("songs/2.dspx") &&
                               harness.settingsWrites == 12,
                           QStringLiteral("recent add must retain ten newest entries in order"));
            });
            log.run(operationId, QStringLiteral("INVALID-PATH"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().addRecentProjectFile(
                    applicationContext(), QStringLiteral("   "));
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("blank recent path must retain operation ID"));
                log.expect(harness.settingsWriteAttempts == 0,
                           QStringLiteral("blank recent path must not persist"));
            });
            log.run(operationId, QStringLiteral("PERSISTENCE-FAILURE"), [&] {
                RuntimeHarness harness;
                harness.settingsApplySucceeds = false;
                const auto result = harness.core().settings().addRecentProjectFile(
                    applicationContext(), QStringLiteral("projects/failure.dspx"));
                log.expectError(result, Automation::AutomationErrorCode::IoError, operationId,
                                QStringLiteral("recent add persistence failure must be stable"));
                log.expect(harness.settings.general.recentProjectFiles.isEmpty() &&
                               harness.settingsWrites == 0,
                           QStringLiteral("failed recent add must preserve storage"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().settings().addRecentProjectFile(
                    applicationContext(), QStringLiteral("projects/a.dspx"));
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing recent add host must be explicit"));
            });
        }

        void runRecentRemove(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::recent_files::remove);
            log.run(operationId, QStringLiteral("NORMALIZE"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {
                    QStringLiteral("projects/歌曲.dspx")};
                const auto result = harness.core().settings().removeRecentProjectFile(
                    applicationContext(), QStringLiteral(" projects/./歌曲.dspx "));
                log.expect(result && result.get().changed &&
                               harness.settings.general.recentProjectFiles.isEmpty() &&
                               harness.settingsWrites == 1,
                           QStringLiteral("recent remove must normalize its target"));
            });
            log.run(operationId, QStringLiteral("MISSING-NO-OP"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().removeRecentProjectFile(
                    applicationContext(), QStringLiteral("projects/missing.dspx"));
                log.expect(result && !result.get().changed && harness.settingsWriteAttempts == 0,
                           QStringLiteral("missing recent path must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("projects/a.dspx")};
                const auto result = harness.core().settings().removeRecentProjectFile(
                    applicationContext(true), QStringLiteral("projects/a.dspx"));
                log.expect(result && result.get().validatedOnly && result.get().changed &&
                               harness.settings.general.recentProjectFiles.size() == 1 &&
                               harness.settingsWriteAttempts == 0,
                           QStringLiteral("recent remove preview must not persist"));
            });
            log.run(operationId, QStringLiteral("INVALID-PATH"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().removeRecentProjectFile(
                    applicationContext(), QStringLiteral(" "));
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("blank recent removal must retain operation ID"));
            });
            log.run(operationId, QStringLiteral("PERSISTENCE-FAILURE"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("projects/a.dspx")};
                harness.settingsApplySucceeds = false;
                const auto result = harness.core().settings().removeRecentProjectFile(
                    applicationContext(), QStringLiteral("projects/a.dspx"));
                log.expectError(result, Automation::AutomationErrorCode::IoError, operationId,
                                QStringLiteral("recent remove persistence failure must be stable"));
                log.expect(harness.settings.general.recentProjectFiles.size() == 1,
                           QStringLiteral("failed recent remove must preserve storage"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().settings().removeRecentProjectFile(
                    applicationContext(), QStringLiteral("projects/a.dspx"));
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing recent remove host must be explicit"));
            });
            log.run(operationId, QStringLiteral("REPEATED-REQUEST-POLICY"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("projects/a.dspx")};
                const auto first = harness.core().settings().removeRecentProjectFile(
                    applicationContext(), QStringLiteral("projects/a.dspx"));
                const auto second = harness.core().settings().removeRecentProjectFile(
                    applicationContext(), QStringLiteral("projects/a.dspx"));
                log.expect(first && first.get().changed && second && !second.get().changed &&
                               harness.settingsWrites == 1,
                           QStringLiteral("repeated recent removal must persist once"));
            });
        }

        void runRecentClear(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::recent_files::clear);
            log.run(operationId, QStringLiteral("NORMAL"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("a.dspx"),
                                                               QStringLiteral("b.dspx")};
                const auto result =
                    harness.core().settings().clearRecentProjectFiles(applicationContext());
                log.expect(result && result.get().changed &&
                               harness.settings.general.recentProjectFiles.isEmpty() &&
                               harness.settingsWrites == 1,
                           QStringLiteral("recent clear must remove the whole ordered snapshot"));
            });
            log.run(operationId, QStringLiteral("EMPTY-NO-OP"), [&] {
                RuntimeHarness harness;
                const auto result =
                    harness.core().settings().clearRecentProjectFiles(applicationContext());
                log.expect(result && !result.get().changed && harness.settingsWriteAttempts == 0,
                           QStringLiteral("clearing empty recent list must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("a.dspx")};
                const auto result =
                    harness.core().settings().clearRecentProjectFiles(applicationContext(true));
                log.expect(result && result.get().validatedOnly && result.get().changed &&
                               harness.settings.general.recentProjectFiles.size() == 1 &&
                               harness.settingsWriteAttempts == 0,
                           QStringLiteral("recent clear preview must not persist"));
            });
            log.run(operationId, QStringLiteral("PERSISTENCE-FAILURE"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("a.dspx")};
                harness.settingsApplySucceeds = false;
                const auto result =
                    harness.core().settings().clearRecentProjectFiles(applicationContext());
                log.expectError(result, Automation::AutomationErrorCode::IoError, operationId,
                                QStringLiteral("recent clear persistence failure must be stable"));
                log.expect(harness.settings.general.recentProjectFiles.size() == 1,
                           QStringLiteral("failed recent clear must preserve storage"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result =
                    harness.core().settings().clearRecentProjectFiles(applicationContext());
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing recent clear host must be explicit"));
            });
            log.run(operationId, QStringLiteral("REPEATED-REQUEST-POLICY"), [&] {
                RuntimeHarness harness;
                harness.settings.general.recentProjectFiles = {QStringLiteral("a.dspx")};
                const auto first =
                    harness.core().settings().clearRecentProjectFiles(applicationContext());
                const auto second =
                    harness.core().settings().clearRecentProjectFiles(applicationContext());
                log.expect(first && first.get().changed && second && !second.get().changed &&
                               harness.settingsWrites == 1,
                           QStringLiteral("repeated clear must persist once"));
            });
        }

        void runSearchPathQuery(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::packages::get_search_paths);
            log.run(operationId, QStringLiteral("EMPTY"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().getPackageSearchPaths();
                log.expect(result && result.get().isEmpty(),
                           QStringLiteral("package path query must preserve empty state"));
            });
            log.run(operationId, QStringLiteral("ORDERED-UNICODE"), [&] {
                RuntimeHarness harness;
                harness.settings.general.packageSearchPaths = {QStringLiteral("voices/主声库"),
                                                               QStringLiteral("voices/secondary")};
                const auto result = harness.core().settings().getPackageSearchPaths();
                log.expect(result && result.get() == harness.settings.general.packageSearchPaths,
                           QStringLiteral("package path query must preserve Unicode order"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                harness.settings.general.packageSearchPaths = {QStringLiteral("voices/a")};
                const auto result = harness.core().settings().getPackageSearchPaths();
                harness.settings.general.packageSearchPaths.clear();
                log.expect(result && result.get() == QStringList{QStringLiteral("voices/a")},
                           QStringLiteral("package path query must return detached values"));
            });
            log.run(operationId, QStringLiteral("DETERMINISTIC"), [&] {
                RuntimeHarness harness;
                harness.settings.general.packageSearchPaths = {QStringLiteral("voices/a")};
                const auto first = harness.core().settings().getPackageSearchPaths();
                const auto second = harness.core().settings().getPackageSearchPaths();
                log.expect(first && second && first.get() == second.get(),
                           QStringLiteral("package path query must be deterministic"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().settings().getPackageSearchPaths();
                log.expect(result && harness.settingsWriteAttempts == 0 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("package path query must not persist or revise"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().settings().getPackageSearchPaths();
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing package path host must be decorated"));
            });
        }

        void runSetSearchPaths(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::packages::set_search_paths);
            log.run(operationId, QStringLiteral("NORMALIZE-UNICODE"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().setPackageSearchPaths(
                    applicationContext(),
                    {QStringLiteral(" packages/../voices/主声库 "), QStringLiteral("voices/主声库"),
                     QStringLiteral(" "), QStringLiteral("voices/secondary")});
                log.expect(
                    result && result.get().changed &&
                        harness.settings.general.packageSearchPaths ==
                            QStringList{QStringLiteral("voices/主声库"),
                                        QStringLiteral("voices/secondary")} &&
                        harness.settingsWrites == 1,
                    QStringLiteral("search paths must normalize, deduplicate and preserve order"));
            });
            log.run(operationId, QStringLiteral("EMPTY-CLEAR"), [&] {
                RuntimeHarness harness;
                harness.settings.general.packageSearchPaths = {QStringLiteral("voices/a")};
                const auto result =
                    harness.core().settings().setPackageSearchPaths(applicationContext(), {});
                log.expect(result && result.get().changed &&
                               harness.settings.general.packageSearchPaths.isEmpty(),
                           QStringLiteral("empty path list must explicitly clear search paths"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                harness.settings.general.packageSearchPaths = {QStringLiteral("voices/a")};
                const auto result = harness.core().settings().setPackageSearchPaths(
                    applicationContext(), {QStringLiteral("voices/./a")});
                log.expect(result && !result.get().changed && harness.settingsWriteAttempts == 0,
                           QStringLiteral("equivalent search paths must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().setPackageSearchPaths(
                    applicationContext(true), {QStringLiteral("voices/主声库")});
                log.expect(result && result.get().validatedOnly && result.get().changed &&
                               harness.settings.general.packageSearchPaths.isEmpty() &&
                               harness.settingsWriteAttempts == 0,
                           QStringLiteral("search path preview must not persist"));
            });
            log.run(operationId, QStringLiteral("CASE-FOLD-DEDUP"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().settings().setPackageSearchPaths(
                    applicationContext(),
                    {QStringLiteral("Voices/Main"), QStringLiteral("voices/main")});
                log.expect(
                    result && harness.settings.general.packageSearchPaths ==
                                  QStringList{QStringLiteral("Voices/Main")},
                    QStringLiteral("Windows search paths must deduplicate case-insensitively"));
            });
            log.run(operationId, QStringLiteral("PERSISTENCE-FAILURE"), [&] {
                RuntimeHarness harness;
                harness.settings.general.packageSearchPaths = {QStringLiteral("voices/original")};
                harness.settingsApplySucceeds = false;
                const auto result = harness.core().settings().setPackageSearchPaths(
                    applicationContext(), {QStringLiteral("voices/new")});
                log.expectError(result, Automation::AutomationErrorCode::IoError, operationId,
                                QStringLiteral("search path persistence failure must be stable"));
                log.expect(harness.settings.general.packageSearchPaths ==
                               QStringList{QStringLiteral("voices/original")},
                           QStringLiteral("failed search path write must preserve storage"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().settings().setPackageSearchPaths(
                    applicationContext(), {QStringLiteral("voices/a")});
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing search path host must be explicit"));
            });
            log.run(operationId, QStringLiteral("REPEATED-REQUEST-POLICY"), [&] {
                RuntimeHarness harness;
                const QStringList paths{QStringLiteral("voices/a"), QStringLiteral("voices/b")};
                const auto first =
                    harness.core().settings().setPackageSearchPaths(applicationContext(), paths);
                const auto second =
                    harness.core().settings().setPackageSearchPaths(applicationContext(), paths);
                log.expect(first && first.get().changed && second && !second.get().changed &&
                               harness.settingsWrites == 1,
                           QStringLiteral("repeated search path request must persist once"));
            });
        }

        void runPackageList(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::packages::list);
            log.run(operationId, QStringLiteral("TYPED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().packages().getInstalledPackages();
                log.expect(
                    result && result.get() == harness.packages && result.get().size() == 1 &&
                        result.get().first().singers.size() == 1,
                    QStringLiteral("package list must return typed package and singer DTOs"));
            });
            log.run(operationId, QStringLiteral("UNICODE"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().packages().getInstalledPackages();
                log.expect(result && result.get().first().id.contains(QStringLiteral("主唱")) &&
                               result.get().first().singers.first().name == QStringLiteral("主唱"),
                           QStringLiteral("package list must preserve Unicode metadata"));
            });
            log.run(operationId, QStringLiteral("EMPTY"), [&] {
                RuntimeHarness harness;
                harness.packages.clear();
                const auto result = harness.core().packages().getInstalledPackages();
                log.expect(result && result.get().isEmpty(),
                           QStringLiteral("package list must preserve empty registry"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().packages().getInstalledPackages();
                const auto captured = result ? result.get() : QList<Automation::PackageDto>{};
                harness.packages.clear();
                log.expect(result && result.get() == captured && !result.get().isEmpty(),
                           QStringLiteral("package list must be detached from registry"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().packages().getInstalledPackages();
                log.expect(result && harness.core().documentVersion() == version &&
                               harness.hostCalls.value(operationId) == 1,
                           QStringLiteral("package list must query once without revision"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().packages().getInstalledPackages();
                log.expectError(result, Automation::AutomationErrorCode::ModuleNotReady,
                                operationId,
                                QStringLiteral("missing package registry must be decorated"));
            });
        }

        void runPackageValidate(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::packages::validate);
            log.run(operationId, QStringLiteral("REPORT"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().packages().validatePackage(
                    QStringLiteral("packages/candidate.dspk"));
                log.expect(result && result.get() == harness.packageReport &&
                               harness.hostCalls.value(operationId) == 1,
                           QStringLiteral("package validation must preserve backend report"));
            });
            log.run(operationId, QStringLiteral("UNICODE-PATH"), [&] {
                RuntimeHarness harness;
                const auto path = QStringLiteral(" packages/候选 声库.dspk ");
                const auto result = harness.core().packages().validatePackage(path);
                log.expect(result && harness.lastValidatedPackagePath == path,
                           QStringLiteral("package validator must receive the exact Unicode path"));
            });
            log.run(operationId, QStringLiteral("DETACHED-REPORT"), [&] {
                RuntimeHarness harness;
                const auto result =
                    harness.core().packages().validatePackage(QStringLiteral("packages/a.dspk"));
                harness.packageReport.items.clear();
                log.expect(result && result.get().items.size() == 1,
                           QStringLiteral("validation report must be detached from backend state"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result =
                    harness.core().packages().validatePackage(QStringLiteral("packages/a.dspk"));
                log.expect(
                    result && harness.core().documentVersion() == version &&
                        harness.settingsWriteAttempts == 0 && harness.presetWrites == 0,
                    QStringLiteral("package validation query must not mutate runtime state"));
            });
            log.run(operationId, QStringLiteral("EMPTY-PATH"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().packages().validatePackage(QStringLiteral(" "));
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId, QStringLiteral("blank package path must be rejected"));
                log.expect(harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("blank package path must not call backend"));
            });
            log.run(operationId, QStringLiteral("BACKEND-FAILURE"), [&] {
                RuntimeHarness harness;
                harness.packageValidationSucceeds = false;
                const auto result = harness.core().packages().validatePackage(
                    QStringLiteral("packages/broken.dspk"));
                log.expectError(result, Automation::AutomationErrorCode::IoError, operationId,
                                QStringLiteral("package backend error must be decorated"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result =
                    harness.core().packages().validatePackage(QStringLiteral("packages/a.dspk"));
                log.expectError(result, Automation::AutomationErrorCode::ModuleNotReady,
                                operationId,
                                QStringLiteral("missing package validator must be explicit"));
            });
        }

        void runPackageResolve(ScenarioLog &log) {
            const auto operationId = Automation::OperationId(
                Automation::OperationIds::packages::resolve_document_voices);
            log.run(operationId, QStringLiteral("NORMAL"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result =
                    harness.core().packages().resolveDocumentVoices(commandContext(harness));
                log.expect(result && result.get().changed &&
                               harness.packageResolveApplyCalls == 1 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("voice resolution must apply without document revision"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                harness.packageResolveCount = 0;
                const auto result =
                    harness.core().packages().resolveDocumentVoices(commandContext(harness));
                log.expect(result && !result.get().changed &&
                               harness.packageResolveApplyCalls == 1 &&
                               harness.core().documentVersion().revision == 0,
                           QStringLiteral("zero resolved voices must be a successful no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result =
                    harness.core().packages().resolveDocumentVoices(commandContext(harness, true));
                log.expect(
                    result && result.get().validatedOnly && result.get().changed &&
                        harness.packageResolvePreviewCalls == 1 &&
                        harness.packageResolveApplyCalls == 0 &&
                        harness.core().documentVersion() == version,
                    QStringLiteral("voice resolution preview must use non-applying host path"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                ++context.expected.revision;
                const auto result = harness.core().packages().resolveDocumentVoices(context);
                log.expectError(
                    result, Automation::AutomationErrorCode::DocumentChanged, operationId,
                    QStringLiteral("voice resolution must prioritize document identity"));
                log.expect(harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("document failure must not call voice resolver"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                ++context.expected.revision;
                const auto result = harness.core().packages().resolveDocumentVoices(context);
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId,
                                QStringLiteral("voice resolution must reject stale revision"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result =
                    harness.core().packages().resolveDocumentVoices(commandContext(harness));
                log.expectError(result, Automation::AutomationErrorCode::ModuleNotReady,
                                operationId,
                                QStringLiteral("missing voice resolver must be explicit"));
            });
            log.run(operationId, QStringLiteral("IDEMPOTENCY-REJECTED"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().packages().resolveDocumentVoices(
                    commandContext(harness, false, QStringLiteral("package-resolution-key")));
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("cache-only voice resolution must reject keys"));
                log.expect(harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("rejected key must not call voice resolver"));
            });
            log.run(operationId, QStringLiteral("SINGLE-HOST-CALL"), [&] {
                RuntimeHarness harness;
                const auto result =
                    harness.core().packages().resolveDocumentVoices(commandContext(harness));
                log.expect(result && harness.hostCalls.value(operationId) == 1,
                           QStringLiteral("one voice resolution request must call host once"));
            });
        }

        void runPresetList(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::speaker_mix_presets::list);
            log.run(operationId, QStringLiteral("EMPTY"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().presets().getSpeakerMixPresets();
                log.expect(result && result.get().isEmpty(),
                           QStringLiteral("preset list must preserve empty storage"));
            });
            log.run(operationId, QStringLiteral("TYPED-UNICODE"), [&] {
                RuntimeHarness harness;
                auto preset = validPreset(QStringLiteral("主唱预设"));
                preset.id = QStringLiteral("preset-id");
                harness.presets = {preset};
                const auto result = harness.core().presets().getSpeakerMixPresets();
                log.expect(result && result.get() == QList<Automation::SpeakerMixPresetDto>{preset},
                           QStringLiteral("preset list must preserve typed Unicode values"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                auto preset = validPreset();
                preset.id = QStringLiteral("preset-id");
                harness.presets = {preset};
                const auto result = harness.core().presets().getSpeakerMixPresets();
                harness.presets.first().name = QStringLiteral("changed");
                log.expect(result && result.get().first().name == preset.name,
                           QStringLiteral("preset list must be detached from storage"));
            });
            log.run(operationId, QStringLiteral("DETERMINISTIC"), [&] {
                RuntimeHarness harness;
                harness.presets = {validPreset(QStringLiteral("A")),
                                   validPreset(QStringLiteral("B"))};
                const auto first = harness.core().presets().getSpeakerMixPresets();
                const auto second = harness.core().presets().getSpeakerMixPresets();
                log.expect(first && second && first.get() == second.get(),
                           QStringLiteral("preset list order must be deterministic"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().presets().getSpeakerMixPresets();
                log.expect(result && harness.presetWriteAttempts == 0 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("preset list must not write or revise document"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().presets().getSpeakerMixPresets();
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing preset list host must be decorated"));
            });
        }

        void runPresetSave(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::speaker_mix_presets::save);
            log.run(operationId, QStringLiteral("VALIDATE-CREATE"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().presets().saveSpeakerMixPreset(
                    applicationContext(true), validPreset(QStringLiteral("主唱预设")));
                log.expect(result && result.get().id.isEmpty() &&
                               !result.get().createdAt.isValid() &&
                               !result.get().updatedAt.isValid() && harness.presets.isEmpty() &&
                               harness.presetWriteAttempts == 0,
                           QStringLiteral("preset preview must not allocate metadata or persist"));
            });
            log.run(operationId, QStringLiteral("CREATE-UNICODE"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().presets().saveSpeakerMixPreset(
                    applicationContext(), validPreset(QStringLiteral("主唱预设")));
                log.expect(
                    result && !result.get().id.isEmpty() && result.get().createdAt.isValid() &&
                        result.get().updatedAt.isValid() &&
                        result.get().name == QStringLiteral("主唱预设") &&
                        harness.presets == QList<Automation::SpeakerMixPresetDto>{result.get()} &&
                        harness.presetWrites == 1 && harness.core().documentVersion() == version,
                    QStringLiteral("preset create must allocate once and stay outside document"));
            });
            log.run(operationId, QStringLiteral("UPDATE"), [&] {
                RuntimeHarness harness;
                auto existing = validPreset(QStringLiteral("Lead"));
                existing.id = QStringLiteral("preset-existing");
                existing.createdAt = QDateTime::fromSecsSinceEpoch(100, Qt::UTC);
                existing.updatedAt = existing.createdAt;
                harness.presets = {existing};
                auto update = existing;
                update.name = QStringLiteral("主唱更新");
                update.createdAt = {};
                const auto result =
                    harness.core().presets().saveSpeakerMixPreset(applicationContext(), update);
                log.expect(
                    result && result.get().id == existing.id &&
                        result.get().createdAt == existing.createdAt &&
                        result.get().name == QStringLiteral("主唱更新") &&
                        harness.presets.size() == 1 && harness.presetWrites == 1,
                    QStringLiteral("preset update must preserve identity and creation time"));
            });
            log.run(operationId, QStringLiteral("DUPLICATE-NAME"), [&] {
                RuntimeHarness harness;
                auto existing = validPreset(QStringLiteral("Lead"));
                existing.id = QStringLiteral("existing");
                harness.presets = {existing};
                auto duplicate = validPreset(QStringLiteral("Lead"));
                const auto result =
                    harness.core().presets().saveSpeakerMixPreset(applicationContext(), duplicate);
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("duplicate same-singer name must be rejected"));
                log.expect(harness.presetWriteAttempts == 0,
                           QStringLiteral("duplicate name must not persist"));
            });
            log.run(operationId, QStringLiteral("INVALID-BOUNDARIES"), [&] {
                RuntimeHarness harness;
                auto empty = validPreset();
                empty.name.clear();
                auto mismatched = validPreset();
                mismatched.fixedWeights.removeLast();
                auto nonFinite = validPreset();
                nonFinite.fixedWeights.first() = std::numeric_limits<double>::quiet_NaN();
                const auto emptyResult =
                    harness.core().presets().saveSpeakerMixPreset(applicationContext(), empty);
                const auto mismatchResult =
                    harness.core().presets().saveSpeakerMixPreset(applicationContext(), mismatched);
                const auto nonFiniteResult =
                    harness.core().presets().saveSpeakerMixPreset(applicationContext(), nonFinite);
                log.expectError(emptyResult, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("empty preset identity must be rejected"));
                log.expectError(mismatchResult, Automation::AutomationErrorCode::InvalidArgument,
                                operationId, QStringLiteral("mismatched weights must be rejected"));
                log.expectError(nonFiniteResult, Automation::AutomationErrorCode::InvalidArgument,
                                operationId, QStringLiteral("NaN preset weight must be rejected"));
            });
            log.run(operationId, QStringLiteral("WEIGHT-NORMALIZATION"), [&] {
                RuntimeHarness harness;
                auto preset = validPreset(QStringLiteral("Normalize"));
                preset.sources.append({.speakerId = QStringLiteral("speaker-c"),
                                       .speakerName = QStringLiteral("Speaker C")});
                preset.fixedWeights = {0.8, 0.8};
                const auto result =
                    harness.core().presets().saveSpeakerMixPreset(applicationContext(), preset);
                const bool normalized = result && result.get().fixedWeights.size() == 2 &&
                                        std::abs(result.get().fixedWeights.at(0) - 0.5) < 1e-12 &&
                                        std::abs(result.get().fixedWeights.at(1) - 0.5) < 1e-12;
                log.expect(normalized,
                           QStringLiteral("preset save must normalize positive fixed weights"));
            });
            log.run(operationId, QStringLiteral("PERSISTENCE-FAILURE"), [&] {
                RuntimeHarness harness;
                harness.presetApplySucceeds = false;
                const auto result = harness.core().presets().saveSpeakerMixPreset(
                    applicationContext(), validPreset(QStringLiteral("Failure")));
                log.expectError(result, Automation::AutomationErrorCode::IoError, operationId,
                                QStringLiteral("preset save persistence failure must be stable"));
                log.expect(harness.presets.isEmpty() && harness.presetWriteAttempts == 1 &&
                               harness.presetWrites == 0,
                           QStringLiteral("failed preset save must preserve storage"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().presets().saveSpeakerMixPreset(
                    applicationContext(), validPreset());
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing preset save host must be explicit"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-ID"), [&] {
                RuntimeHarness harness;
                auto update = validPreset(QStringLiteral("Unknown update"));
                update.id = QStringLiteral("missing-preset-id");
                const auto result =
                    harness.core().presets().saveSpeakerMixPreset(applicationContext(), update);
                log.expectError(
                    result, Automation::AutomationErrorCode::NotFound, operationId,
                    QStringLiteral("unknown non-empty preset ID must not create a record"));
                log.expect(harness.presets.isEmpty() && harness.presetWriteAttempts == 0,
                           QStringLiteral("unknown preset update must not persist"));
            });
        }

        void runPresetDelete(ScenarioLog &log) {
            const auto operationId = Automation::OperationId(
                Automation::OperationIds::speaker_mix_presets::delete_preset);
            log.run(operationId, QStringLiteral("NORMAL"), [&] {
                RuntimeHarness harness;
                auto preset = validPreset();
                preset.id = QStringLiteral("preset-id");
                harness.presets = {preset};
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().presets().deleteSpeakerMixPreset(
                    applicationContext(), preset.id);
                log.expect(result && result.get().changed && harness.presets.isEmpty() &&
                               harness.presetWrites == 1 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("preset delete must persist once outside document"));
            });
            log.run(operationId, QStringLiteral("MISSING-NO-OP"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().presets().deleteSpeakerMixPreset(
                    applicationContext(), QStringLiteral("missing"));
                log.expect(result && !result.get().changed && harness.presetWriteAttempts == 0,
                           QStringLiteral("missing preset delete must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                auto preset = validPreset();
                preset.id = QStringLiteral("preset-id");
                harness.presets = {preset};
                const auto result = harness.core().presets().deleteSpeakerMixPreset(
                    applicationContext(true), preset.id);
                log.expect(result && result.get().validatedOnly && result.get().changed &&
                               harness.presets.size() == 1 && harness.presetWriteAttempts == 0,
                           QStringLiteral("preset delete preview must not persist"));
            });
            log.run(operationId, QStringLiteral("INVALID-ID"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().presets().deleteSpeakerMixPreset(
                    applicationContext(), QStringLiteral(" "));
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("blank preset ID must retain operation ID"));
            });
            log.run(operationId, QStringLiteral("DUPLICATE-ID-CLEANUP"), [&] {
                RuntimeHarness harness;
                auto first = validPreset(QStringLiteral("First"));
                first.id = QStringLiteral("duplicate-id");
                auto second = validPreset(QStringLiteral("Second"));
                second.id = first.id;
                harness.presets = {first, second};
                const auto result =
                    harness.core().presets().deleteSpeakerMixPreset(applicationContext(), first.id);
                log.expect(result && result.get().changed && harness.presets.isEmpty() &&
                               harness.presetWrites == 1,
                           QStringLiteral("delete must clean all duplicate stored IDs atomically"));
            });
            log.run(operationId, QStringLiteral("PERSISTENCE-FAILURE"), [&] {
                RuntimeHarness harness;
                auto preset = validPreset();
                preset.id = QStringLiteral("preset-id");
                harness.presets = {preset};
                harness.presetApplySucceeds = false;
                const auto result = harness.core().presets().deleteSpeakerMixPreset(
                    applicationContext(), preset.id);
                log.expectError(result, Automation::AutomationErrorCode::IoError, operationId,
                                QStringLiteral("preset delete persistence failure must be stable"));
                log.expect(harness.presets.size() == 1 && harness.presetWrites == 0,
                           QStringLiteral("failed preset delete must preserve storage"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().presets().deleteSpeakerMixPreset(
                    applicationContext(), QStringLiteral("preset-id"));
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing preset delete host must be explicit"));
            });
            log.run(operationId, QStringLiteral("REPEATED-REQUEST-POLICY"), [&] {
                RuntimeHarness harness;
                auto preset = validPreset();
                preset.id = QStringLiteral("preset-id");
                harness.presets = {preset};
                const auto first = harness.core().presets().deleteSpeakerMixPreset(
                    applicationContext(), preset.id);
                const auto second = harness.core().presets().deleteSpeakerMixPreset(
                    applicationContext(), preset.id);
                log.expect(first && first.get().changed && second && !second.get().changed &&
                               harness.presetWrites == 1,
                           QStringLiteral("repeated preset delete must persist once"));
            });
        }
    }

    void runSettingsPackageDimensions(ScenarioLog &log) {
        runSettingsQuery(log);
        runSettingsCommands(log);
        runRecentList(log);
        runRecentAdd(log);
        runRecentRemove(log);
        runRecentClear(log);
        runSearchPathQuery(log);
        runSetSearchPaths(log);
        runPackageList(log);
        runPackageValidate(log);
        runPackageResolve(log);
        runPresetList(log);
        runPresetSave(log);
        runPresetDelete(log);
    }

} // namespace RuntimeDimensions
