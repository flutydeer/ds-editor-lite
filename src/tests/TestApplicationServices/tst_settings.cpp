#include "tst_application_services.h"
#include "ApplicationHarness.h"

#include <QtTest>
#include <limits>

using namespace ApplicationTest;

Q_DECLARE_METATYPE(Automation::AudioDeviceSettingsPatchDto)
Q_DECLARE_METATYPE(Automation::ComputeDeviceSettingsPatchDto)

namespace {
    template <typename Update>
    void exerciseDeviceSelection(ApplicationHarness &harness, Update update,
                                 const Automation::SettingsSnapshotDto &expected,
                                 const QString &rejectedField,
                                 const QStringList &restartFields = {}) {
        const auto before = harness.settings;
        const auto version = harness.core().documentVersion();
        const auto *undoBefore = HistoryManager::instance()->nextUndoEntry();
        const auto preview = update(applicationContext(true));
        QCOMPARE(harness.settings, before);
        QCOMPARE(harness.settingsWriteAttempts, 0);
        const auto committed = update(applicationContext());
        if (!rejectedField.isEmpty()) {
            for (const auto *result : {&preview, &committed}) {
                QVERIFY(!*result);
                QCOMPARE(result->getError().code, Automation::AutomationErrorCode::InvalidArgument);
                QCOMPARE(result->getError().fieldPath, rejectedField);
            }
            QCOMPARE(harness.settingsWriteAttempts, 0);
            QCOMPARE(harness.settings, before);
        } else {
            QVERIFY(preview && preview.get().validatedOnly && preview.get().changed);
            QVERIFY(committed && committed.get().changed && !committed.get().validatedOnly);
            QCOMPARE(preview.get().restartRequiredFields, restartFields);
            QCOMPARE(committed.get().restartRequiredFields, restartFields);
            QCOMPARE(committed.get().restartRequired, !restartFields.isEmpty());
            QCOMPARE(harness.settings, expected);
            const auto repeated = update(applicationContext());
            QVERIFY(repeated && !repeated.get().changed && !repeated.get().restartRequired);
            QCOMPARE(harness.settingsWriteAttempts, 1);
            QCOMPARE(harness.settingsWrites, 1);
        }
        QCOMPARE(harness.core().documentVersion(), version);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undoBefore);
    }
}

void ApplicationServicesTests::settingsPathProjection() {
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

void ApplicationServicesTests::sparseSettingsUpdatesPreserveOtherValues() {
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
    const auto render =
        runtime.settings().updateRender(applicationContext(), {.samplingSteps = 32,
                                                               .runVocoderOnCpu = true,
                                                               .autoStartInference = false,
                                                               .playbackLookaheadSeconds = 12.5,
                                                               .pitchSmoothKernelSize = 9});
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
    expected.inference.autoStartInference = false;
    expected.inference.playbackLookaheadSeconds = 12.5;
    expected.inference.pitchSmoothKernelSize = 9;
    expected.inference.singerSessionCacheCapacity = 2;
    expected.inference.singerSessionIdleTimeoutSeconds = 120;
    QVERIFY(harness.settings == expected);
}

void ApplicationServicesTests::sparseSettingsPreviewAndFailure() {
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

void ApplicationServicesTests::audioDeviceSettingsFollowAvailableCandidates_data() {
    QTest::addColumn<Automation::AudioDeviceSettingsPatchDto>("patch");
    QTest::addColumn<QString>("rejectedField");
    const Automation::AudioDeviceSettingsPatchDto available{
        .driverName = QStringLiteral(" test-driver "),
        .deviceName = QStringLiteral(" test-device "),
        .bufferSize = 256,
        .sampleRate = 48000.0,
    };
    QTest::newRow("supported-device-format") << available << QString{};
    auto unavailableDriver = available;
    unavailableDriver.driverName = QStringLiteral("offline-driver");
    QTest::newRow("unavailable-driver") << unavailableDriver << QStringLiteral("driver_name");
    auto unavailableDevice = available;
    unavailableDevice.deviceName = QStringLiteral("offline-device");
    QTest::newRow("unavailable-device") << unavailableDevice << QStringLiteral("device_name");
    auto unsupportedBuffer = available;
    unsupportedBuffer.bufferSize = 1024;
    QTest::newRow("unsupported-device-buffer")
        << unsupportedBuffer << QStringLiteral("buffer_size");
    auto unsupportedRate = available;
    unsupportedRate.sampleRate = 96000.0;
    QTest::newRow("unsupported-device-sample-rate")
        << unsupportedRate << QStringLiteral("sample_rate");
}

void ApplicationServicesTests::audioDeviceSettingsFollowAvailableCandidates() {
    QFETCH(Automation::AudioDeviceSettingsPatchDto, patch);
    QFETCH(QString, rejectedField);
    Automation::PublicSettingsSnapshotDto candidates;
    candidates.audioDevice = Automation::AudioDevicePublicSettingsDto{
        .drivers =
            {
                      {.id = QStringLiteral("test-driver"),
                 .devices =
                     {
                         {.id = QStringLiteral("test-device"),
                          .bufferSizes = {256, 512},
                          .sampleRates = {44100.0, 48000.0}},
                         {.id = QStringLiteral("offline-device"), .available = false},
                     }},
                      {.id = QStringLiteral("offline-driver"), .available = false},
                      },
    };
    ApplicationHarness harness(Automation::WindowId::create(), [&] { return candidates; });
    auto expected = harness.settings;
    expected.audio.driverName = QStringLiteral("test-driver");
    expected.audio.deviceName = QStringLiteral("test-device");
    expected.audio.adoptedBufferSize = 256;
    expected.audio.adoptedSampleRate = 48000.0;
    exerciseDeviceSelection(
        harness,
        [&](const auto &command) {
            return harness.core().settings().updateAudioDevice(command, patch);
        },
        expected, rejectedField);
}

void ApplicationServicesTests::computeDeviceSettingsMatchAvailableGpuIdentity_data() {
    QTest::addColumn<Automation::ComputeDeviceSettingsPatchDto>("patch");
    QTest::addColumn<QString>("rejectedField");
    const Automation::ComputeDeviceSettingsPatchDto available{
        .executionProvider = QStringLiteral("DirectML"),
        .gpuIndex = 1,
        .gpuId = QStringLiteral(" gpu-b "),
    };
    QTest::newRow("matching-gpu-identity") << available << QString{};
    auto mismatchedIndex = available;
    mismatchedIndex.gpuIndex = 0;
    QTest::newRow("gpu-index-does-not-match-id") << mismatchedIndex << QStringLiteral("gpu_id");
    auto unavailableGpu = available;
    unavailableGpu.gpuId = QStringLiteral("offline-gpu");
    QTest::newRow("unavailable-gpu") << unavailableGpu << QStringLiteral("gpu_id");
    auto unavailableProvider = available;
    unavailableProvider.executionProvider = QStringLiteral("CUDA");
    QTest::newRow("unavailable-provider")
        << unavailableProvider << QStringLiteral("execution_provider");
}

void ApplicationServicesTests::computeDeviceSettingsMatchAvailableGpuIdentity() {
    QFETCH(Automation::ComputeDeviceSettingsPatchDto, patch);
    QFETCH(QString, rejectedField);
    Automation::PublicSettingsSnapshotDto candidates;
    candidates.computeDevice = Automation::ComputeDevicePublicSettingsDto{
        .providerCandidates =
            {
                                 {QStringLiteral("CPU"), QStringLiteral("CPU"), true, {}},
                                 {QStringLiteral("DirectML"), QStringLiteral("DirectML"), true, {}},
                                 {QStringLiteral("CUDA"), QStringLiteral("CUDA"), false,
                 QStringLiteral("Unavailable")},
                                 },
        .gpuCandidates =
            {
                                 {0, QStringLiteral("gpu-a"), QStringLiteral("GPU A"), true, {}},
                                 {1, QStringLiteral("gpu-b"), QStringLiteral("GPU B"), true, {}},
                                 {1, QStringLiteral("offline-gpu"), QStringLiteral("Offline GPU"), false, {}},
                                 },
    };
    ApplicationHarness harness(Automation::WindowId::create(), [&] { return candidates; });
    auto &runtime = harness.core();
    const auto before = harness.settings;
    const auto version = runtime.documentVersion();
    const auto *undoBefore = HistoryManager::instance()->nextUndoEntry();
    auto expected = before;
    expected.inference.executionProvider = QStringLiteral("DirectML");
    expected.inference.selectedGpuIndex = 1;
    expected.inference.selectedGpuId = QStringLiteral("gpu-b");
    const QStringList restartFields{QStringLiteral("execution_provider"),
                                    QStringLiteral("gpu_index"), QStringLiteral("gpu_id")};
    exerciseDeviceSelection(
        harness,
        [&](const auto &command) { return runtime.settings().updateComputeDevice(command, patch); },
        expected, rejectedField, restartFields);
    if (QTest::currentTestFailed())
        return;
    if (rejectedField.isEmpty()) {
        candidates.computeDevice->gpuCandidates[1].available = false;
        const auto cpu = runtime.settings().updateComputeDevice(
            applicationContext(), {.executionProvider = QStringLiteral("CPU")});
        QVERIFY(cpu && cpu.get().changed && cpu.get().restartRequired);
        QCOMPARE(cpu.get().restartRequiredFields,
                 QStringList{QStringLiteral("execution_provider")});
        expected.inference.executionProvider = QStringLiteral("CPU");
        QCOMPARE(harness.settings, expected);
        QCOMPARE(harness.settingsWrites, 2);
    }
    QCOMPARE(runtime.documentVersion(), version);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undoBefore);
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

void ApplicationServicesTests::settingsQuerySnapshot() {
    ApplicationHarness harness;
    const auto snapshot = harness.core().settings().getSettings();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get() == harness.settings);
}

void ApplicationServicesTests::generalSettings() {
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

void ApplicationServicesTests::appearanceSettings() {
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

void ApplicationServicesTests::inferenceSettings() {
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

void ApplicationServicesTests::developerSettings() {
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

void ApplicationServicesTests::g2pLanguageSettings() {
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

void ApplicationServicesTests::fillLyricSettings() {
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

void ApplicationServicesTests::windowSettings() {
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

void ApplicationServicesTests::audioSettings() {
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

void ApplicationServicesTests::recentFiles() {
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
