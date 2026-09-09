#include "TestAutomationApplicationDomains.h"
#include "ApplicationHarness.h"

#include <QtTest>
#include <limits>

using namespace ApplicationTest;

void TestAutomationApplicationDomains::settingsPathProjection() {
    ApplicationHarness harness;
    harness.settings.general.packageSearchPaths = {QStringLiteral("allowed/voices"),
                                                   QStringLiteral("private/voices")};
    auto &runtime = harness.core();
    const auto projected =
        runtime.settings().queryPublicSettings({QStringLiteral("package_search_paths")},
                                               [](const QString &path) -> std::optional<QString> {
                                                   if (path.startsWith(QStringLiteral("allowed")))
                                                       return path;
                                                   return std::nullopt;
                                               });
    QVERIFY2((projected && projected.get().packageSearchPaths &&
              projected.get().packageSearchPaths->configured ==
                  QStringList{QStringLiteral("allowed/voices")}),
             qPrintable(QStringLiteral(
                 "settings.query must filter package paths through the projection")));
    const auto unknown =
        runtime.settings().queryPublicSettings({QStringLiteral("not_a_public_domain")});
    QVERIFY2(
        (!unknown && unknown.getError().code == Automation::AutomationErrorCode::InvalidArgument),
        qPrintable(
            QStringLiteral("settings.query must reject domains outside the explicit allowlist")));
}

void TestAutomationApplicationDomains::sparseSettingsUpdatesPreserveOtherValues() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto version = runtime.documentVersion();
    const auto before = harness.settings;
    const auto ui = runtime.settings().updateUiLanguage(applicationContext(),
                                                        {.uiLanguage = QStringLiteral("en_US")});
    const auto singing = runtime.settings().updateSinging(
        applicationContext(),
        {.defaultLanguage = QStringLiteral("eng"),
         .defaultLyrics = QMap<QString, QString>{{QStringLiteral("eng"), QStringLiteral("la")}}});
    const auto theme =
        runtime.settings().updateTheme(applicationContext(), {.themeId = QStringLiteral("dark")});
    const auto audio =
        runtime.settings().updateAudioDevice(applicationContext(), {.gain = 0.75, .pan = -0.25});
    const auto playback =
        runtime.settings().updatePlaybackBehavior(applicationContext(), {.behavior = 1});
    const auto compute = runtime.settings().updateComputeDevice(
        applicationContext(), {.executionProvider = QStringLiteral("DirectML")});
    const auto render = runtime.settings().updateRender(
        applicationContext(), {.samplingSteps = 32, .runVocoderOnCpu = true});
    const auto retention = runtime.settings().updateSingerSessionRetention(
        applicationContext(), {.capacity = 2, .idleTimeoutSeconds = 120});
    QVERIFY2((ui && singing && theme && audio && playback && compute && render && retention),
             qPrintable(QStringLiteral(
                 "all public settings update domains must accept valid sparse updates")));
    QVERIFY2(
        (compute.get().restartRequired &&
         compute.get().restartRequiredFields.contains(QStringLiteral("execution_provider")) &&
         render.get().restartRequired),
        qPrintable(QStringLiteral("restart-only settings must report precise restart fields")));
    QVERIFY2((runtime.documentVersion() == version),
             qPrintable(QStringLiteral("application settings must not change document revision")));


    auto expected = before;
    expected.general.uiLanguage = QStringLiteral("en_US");
    expected.general.defaultSingingLanguage = QStringLiteral("eng");
    expected.general.defaultLyrics = {
        {QStringLiteral("eng"), QStringLiteral("la")}
    };
    expected.appearance.themeId = QStringLiteral("dark");
    expected.audio.deviceGain = 0.75;
    expected.audio.devicePan = -0.25;
    expected.audio.playheadBehavior = 1;
    expected.inference.executionProvider = QStringLiteral("DirectML");
    expected.inference.samplingSteps = 32;
    expected.inference.runVocoderOnCpu = true;
    expected.inference.singerSessionCacheCapacity = 2;
    expected.inference.singerSessionIdleTimeoutSeconds = 120;
    QVERIFY(harness.settings == expected);
}

void TestAutomationApplicationDomains::sparseSettingsPreviewAndFailure() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto preview = runtime.settings().updateUiLanguage(
        applicationContext(true), {.uiLanguage = QStringLiteral("en_US")});
    QVERIFY(preview);
    QVERIFY(preview.get().validatedOnly);
    QVERIFY(preview.get().changed);
    QCOMPARE(harness.settingsWrites, 0);
    QCOMPARE(harness.settings.general.uiLanguage, QStringLiteral("system"));
    const auto beforeFailure = harness.settings;
    harness.settingsApplySucceeds = false;
    const auto failed =
        runtime.settings().updateTheme(applicationContext(), {.themeId = QStringLiteral("light")});
    harness.settingsApplySucceeds = true;
    QVERIFY2((!failed && failed.getError().code == Automation::AutomationErrorCode::IoError &&
              harness.settings == beforeFailure),
             qPrintable(QStringLiteral("settings persistence failure must roll back atomically")));
}

namespace {
    template <typename T, typename Getter, typename Update, typename Mutate>
    void exerciseSettingsCategory(ApplicationHarness &harness,
                                  const Automation::OperationId &operationId, Getter getter,
                                  Update update, Mutate mutate,
                                  const std::function<void(T &)> &invalidate,
                                  const std::function<void(T &)> &mutateFailure) {
        const auto original = getter(harness.settings);
        const auto attemptsBefore = harness.settingsWriteAttempts;
        const auto writesBefore = harness.settingsWrites;
        const auto noOp = update(applicationContext(), original);

        auto target = original;
        mutate(target);
        const auto preview = update(applicationContext(true), target);
        const auto committed = update(applicationContext(), target);
        const auto duplicate = update(applicationContext(), target);
        QVERIFY2((noOp && !noOp.get().changed && preview && preview.get().validatedOnly &&
                  preview.get().changed && committed && committed.get().changed && duplicate &&
                  !duplicate.get().changed && getter(harness.settings) == target &&
                  harness.settingsWriteAttempts == attemptsBefore + 1 &&
                  harness.settingsWrites == writesBefore + 1),
                 qPrintable(QStringLiteral(
                     "settings category must support no-op, preview and one commit")));

        if (invalidate) {
            auto invalidValue = target;
            invalidate(invalidValue);
            const auto invalid = update(applicationContext(), invalidValue);
            QVERIFY2((!invalid &&
                      invalid.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                      invalid.getError().operationId == operationId),
                     qPrintable(QStringLiteral("invalid settings value must be rejected")));
            QVERIFY2((getter(harness.settings) == target),
                     qPrintable(QStringLiteral("invalid settings value must not mutate storage")));
        }

        auto failingValue = target;
        mutateFailure(failingValue);
        harness.settingsApplySucceeds = false;
        const auto failed = update(applicationContext(), failingValue);
        harness.settingsApplySucceeds = true;
        QVERIFY2((!failed && failed.getError().code == Automation::AutomationErrorCode::IoError &&
                  failed.getError().operationId == operationId),
                 qPrintable(QStringLiteral("settings persistence failure must be reported")));
        QVERIFY2(
            (getter(harness.settings) == target && harness.settingsWrites == writesBefore + 1),
            qPrintable(QStringLiteral("failed settings persistence must not alter the snapshot")));
    }

}

void TestAutomationApplicationDomains::settingsQuerySnapshot() {
    ApplicationHarness harness;
    const auto snapshot = harness.core().settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::generalSettings() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    exerciseSettingsCategory<Automation::GeneralSettingsDto>(
        harness, Automation::OperationIds::settings::update_general,
        [](const auto &all) { return all.general; },
        [&runtime](const auto &context, const auto &value) {
            return runtime.settings().updateGeneral(context, value);
        },
        [](auto &value) { value.gameDirectory = QStringLiteral("game-data"); },
        [](auto &value) { value.uiLanguage = QStringLiteral("unsupported"); },
        [](auto &value) { value.pitchModelPath = QStringLiteral("model.bin"); });
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::appearanceSettings() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    exerciseSettingsCategory<Automation::AppearanceSettingsDto>(
        harness, Automation::OperationIds::settings::update_appearance,
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
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::inferenceSettings() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    exerciseSettingsCategory<Automation::InferenceSettingsDto>(
        harness, Automation::OperationIds::settings::update_inference,
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
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::developerSettings() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    exerciseSettingsCategory<Automation::DeveloperSettingsDto>(
        harness, Automation::OperationIds::settings::update_developer,
        [](const auto &all) { return all.developer; },
        [&runtime](const auto &context, const auto &value) {
            return runtime.settings().updateDeveloper(context, value);
        },
        [](auto &value) { value.enableDiagnostics = true; },
        [](auto &value) {
            value.editorRenderBackend = static_cast<Automation::EditorRenderBackend>(99);
        },
        [](auto &value) { value.showLogWindow = true; });
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::g2pLanguageSettings() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    exerciseSettingsCategory<Automation::G2pLanguageSettingsDto>(
        harness, Automation::OperationIds::settings::update_g2p_language,
        [](const auto &all) { return all.g2pLanguage; },
        [&runtime](const auto &context, const auto &value) {
            return runtime.settings().updateG2pLanguage(context, value);
        },
        [](auto &value) { value.languageOrder = {QStringLiteral("cmn"), QStringLiteral("eng")}; },
        [](auto &value) { value.languageOrder = {QStringLiteral("cmn"), QStringLiteral("cmn")}; },
        [](auto &value) { value.languageOrder.append(QStringLiteral("jpn")); });
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::fillLyricSettings() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    exerciseSettingsCategory<Automation::FillLyricSettingsDto>(
        harness, Automation::OperationIds::settings::update_fill_lyric,
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
                .name = QStringLiteral("unicode-tagger"),
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
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::windowSettings() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    exerciseSettingsCategory<Automation::WindowSettingsDto>(
        harness, Automation::OperationIds::settings::update_window,
        [](const auto &all) { return all.window; },
        [&runtime](const auto &context, const auto &value) {
            return runtime.settings().updateWindow(context, value);
        },
        [](auto &value) { value.mainWindowGeometry = QByteArrayLiteral("geometry-one"); }, {},
        [](auto &value) { value.mainWindowGeometry = QByteArrayLiteral("geometry-two"); });
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::audioSettings() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    exerciseSettingsCategory<Automation::AudioSettingsDto>(
        harness, Automation::OperationIds::settings::update_audio,
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
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void TestAutomationApplicationDomains::recentFiles() {
    ApplicationHarness harness;
    auto &runtime = harness.core();

    const auto initial = runtime.settings().getRecentProjectFiles();
    QVERIFY2((initial && initial.get().isEmpty()),
             qPrintable(QStringLiteral("recent file query must preserve an empty list")));

    const auto add = runtime.settings().addRecentProjectFile(
        applicationContext(), QStringLiteral(" projects/../projects/歌曲.dspx "));
    const auto duplicate = runtime.settings().addRecentProjectFile(
        applicationContext(), QStringLiteral("projects/歌曲.dspx"));
    const auto added = runtime.settings().getRecentProjectFiles();
    QVERIFY2((add && add.get().changed && duplicate && !duplicate.get().changed && added &&
              added.get() == QStringList{QStringLiteral("projects/歌曲.dspx")}),
             qPrintable(QStringLiteral("recent add must trim, clean and deduplicate paths")));

    for (int index = 0; index < 12; ++index) {
        const auto result = runtime.settings().addRecentProjectFile(
            applicationContext(), QStringLiteral("projects/song-%1.dspx").arg(index));
        QVERIFY2((bool(result)), qPrintable(QStringLiteral("bulk recent-file setup must succeed")));
    }
    const auto capped = runtime.settings().getRecentProjectFiles();
    QVERIFY2((capped && capped.get().size() == 10 &&
              capped.get().first() == QStringLiteral("projects/song-11.dspx") &&
              capped.get().last() == QStringLiteral("projects/song-2.dspx")),
             qPrintable(QStringLiteral("recent files must keep the ten newest entries in order")));

    const auto remove = runtime.settings().removeRecentProjectFile(
        applicationContext(), QStringLiteral(" projects/song-7.dspx "));
    const auto removeNoOp = runtime.settings().removeRecentProjectFile(
        applicationContext(), QStringLiteral("projects/missing.dspx"));
    QVERIFY2(
        (remove && remove.get().changed && removeNoOp && !removeNoOp.get().changed),
        qPrintable(QStringLiteral("recent remove must normalize and no-op for a missing path")));

    const auto clearPreview = runtime.settings().clearRecentProjectFiles(applicationContext(true));
    const auto clear = runtime.settings().clearRecentProjectFiles(applicationContext());
    const auto clearNoOp = runtime.settings().clearRecentProjectFiles(applicationContext());
    QVERIFY2((clearPreview && clearPreview.get().validatedOnly && clearPreview.get().changed &&
              clear && clear.get().changed && clearNoOp && !clearNoOp.get().changed &&
              harness.settings.general.recentProjectFiles.isEmpty()),
             qPrintable(
                 QStringLiteral("recent clear must preview, commit once and detect empty no-op")));

    const auto invalidAdd =
        runtime.settings().addRecentProjectFile(applicationContext(), QStringLiteral(" "));
    const auto invalidRemove =
        runtime.settings().removeRecentProjectFile(applicationContext(), QStringLiteral(" "));
    QVERIFY2((!invalidAdd &&
              invalidAdd.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              !invalidRemove &&
              invalidRemove.getError().code == Automation::AutomationErrorCode::InvalidArgument),
             qPrintable(QStringLiteral("empty recent paths must be rejected without persistence")));

    harness.settingsApplySucceeds = false;
    const auto failed = runtime.settings().addRecentProjectFile(
        applicationContext(), QStringLiteral("projects/failure.dspx"));
    QVERIFY2((!failed && failed.getError().code == Automation::AutomationErrorCode::IoError &&
              failed.getError().operationId == Automation::OperationIds::recent_files::add),
             qPrintable(QStringLiteral("recent-file persistence failure must be stable")));
    QVERIFY2((harness.settings.general.recentProjectFiles.isEmpty()),
             qPrintable(QStringLiteral("failed recent add must not alter stored files")));
}
