#include "TestAutomationApplicationDomains.h"
#include "ApplicationHarness.h"

#include <QtTest>
#include <limits>

using namespace ApplicationTest;

void TestAutomationApplicationDomains::applicationInfoSnapshot() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto info = runtime.application().getInfo();
    QVERIFY2((info && info.get() == harness.applicationInfo),
             qPrintable(QStringLiteral("application info must be returned as an owned snapshot")));
}

void TestAutomationApplicationDomains::terminationPreviewAndPolicies() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto exitPreview = runtime.application().requestTermination(
        applicationContext(true), Automation::ApplicationTerminationMode::Exit);
    QVERIFY2((exitPreview && exitPreview.get().changed && exitPreview.get().validatedOnly &&
              harness.terminationCalls == 0),
             qPrintable(QStringLiteral("termination preview must not call the host")));

    const auto exit = runtime.application().requestTermination(
        applicationContext(), Automation::ApplicationTerminationMode::Exit);
    QVERIFY2((exit && exit.get().changed && !exit.get().validatedOnly &&
              harness.terminationCalls == 1 &&
              harness.lastTerminationMode == Automation::ApplicationTerminationMode::Exit &&
              harness.lastTerminationSavePolicy ==
                  Automation::ApplicationTerminationSavePolicy::RejectUnsaved),
             qPrintable(QStringLiteral("exit must be mediated exactly once by the host")));

    const auto restart = runtime.application().requestTermination(
        applicationContext(), Automation::ApplicationTerminationMode::Restart, true);
    QVERIFY2(
        (restart && harness.terminationCalls == 2 &&
         harness.lastTerminationMode == Automation::ApplicationTerminationMode::Restart &&
         harness.lastTerminationSavePolicy ==
             Automation::ApplicationTerminationSavePolicy::Discard),
        qPrintable(QStringLiteral("restart must preserve its mode and explicit discard policy")));

    harness.terminationResult = Automation::ApplicationTerminationRequestResult::Accepted;
    const auto guiExit = runtime.application().requestTermination(
        {.source = Automation::InvocationSource::TrustedGui},
        Automation::ApplicationTerminationMode::Exit);
    QVERIFY2((guiExit && harness.lastTerminationSavePolicy ==
                             Automation::ApplicationTerminationSavePolicy::Prompt),
             qPrintable(
                 QStringLiteral("trusted GUI termination must retain interactive prompt policy")));
}

void TestAutomationApplicationDomains::terminationHostErrors() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    harness.terminationResult = Automation::ApplicationTerminationRequestResult::Unavailable;
    const auto rejected = runtime.application().requestTermination(
        applicationContext(), Automation::ApplicationTerminationMode::Exit);
    QVERIFY2(
        (!rejected &&
         rejected.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         rejected.getError().operationId == Automation::OperationIds::application::request_exit),
        qPrintable(QStringLiteral("host rejection must be a stable capability error")));

    harness.terminationResult = Automation::ApplicationTerminationRequestResult::UnsavedChanges;
    const auto unsaved = runtime.application().requestTermination(
        applicationContext(), Automation::ApplicationTerminationMode::Exit);
    QVERIFY2(
        (!unsaved && unsaved.getError().code == Automation::AutomationErrorCode::Busy &&
         unsaved.getError().operationId == Automation::OperationIds::application::request_exit),
        qPrintable(QStringLiteral("unsaved automation exit must be rejected as busy")));
    QVERIFY2((!unsaved && unsaved.getError().fieldPath == QStringLiteral("discard_changes")),
             qPrintable(QStringLiteral("unsaved rejection must identify discard_changes")));

    const auto invalidMode = runtime.application().requestTermination(
        applicationContext(), static_cast<Automation::ApplicationTerminationMode>(99));
    QVERIFY2((!invalidMode &&
              invalidMode.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              invalidMode.getError().operationId.isEmpty()),
             qPrintable(QStringLiteral("invalid mode is rejected before operation routing")));
}

void TestAutomationApplicationDomains::headlessHostCapabilities() {

    ApplicationHarness headlessHarness(std::nullopt);
    auto &headlessRuntime = headlessHarness.core();
    const auto headlessExit = headlessRuntime.application().requestTermination(
        applicationContext(), Automation::ApplicationTerminationMode::Exit);
    const auto headlessGui = headlessRuntime.facade().getEditorState(
        headlessRuntime.documentVersion().documentId, Automation::WindowId::create());
    QVERIFY2((!headlessRuntime.windowId() && headlessExit && headlessHarness.terminationCalls == 1),
             qPrintable(QStringLiteral("headless runtime lifecycle must not require a window ID")));
    QVERIFY2((!headlessGui &&
              headlessGui.getError().code ==
                  Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              headlessGui.getError().operationId == Automation::OperationIds::editor::get_state),
             qPrintable(QStringLiteral("headless runtime must reject GUI routes by capability")));
}

void TestAutomationApplicationDomains::unavailableApplicationHost() {
    AutomationTestSupport::TestRuntime fixture;
    auto &runtime = fixture.runtime();
    const auto info = runtime.application().getInfo();
    const auto exit = runtime.application().requestTermination(
        applicationContext(), Automation::ApplicationTerminationMode::Exit);
    QVERIFY2((!info &&
              info.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              info.getError().operationId == Automation::OperationIds::application::get_info),
             qPrintable(QStringLiteral("missing application info host must be explicit")));
    QVERIFY2((!exit &&
              exit.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              exit.getError().operationId == Automation::OperationIds::application::request_exit),
             qPrintable(QStringLiteral("missing lifecycle host must be explicit")));
}

void TestAutomationApplicationDomains::unavailablePlaybackHost() {
    AutomationTestSupport::TestRuntime fixture;
    auto &runtime = fixture.runtime();
    const auto playback = runtime.playback().getPlayback(runtime.documentVersion().documentId);
    const auto play = runtime.playback().play(commandContext(runtime));
    const auto position = runtime.playback().setPosition(commandContext(runtime), 120.0);
    const auto loop =
        runtime.playback().setLoop(commandContext(runtime), LoopSettings(false, 0, 480));
    QVERIFY2(
        (!playback &&
         playback.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         playback.getError().operationId == Automation::OperationIds::playback::get_state),
        qPrintable(QStringLiteral("missing playback snapshot host must be explicit")));
    QVERIFY2((!play &&
              play.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              play.getError().operationId == Automation::OperationIds::playback::play),
             qPrintable(QStringLiteral("missing playback state host must be explicit")));
    QVERIFY2(
        (!position &&
         position.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         position.getError().operationId == Automation::OperationIds::playback::set_position),
        qPrintable(QStringLiteral("missing playback position host must be explicit")));
    QVERIFY2((!loop &&
              loop.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              loop.getError().operationId == Automation::OperationIds::playback::set_loop),
             qPrintable(QStringLiteral("missing playback loop host must be explicit")));
}

void TestAutomationApplicationDomains::unavailableEditorHost() {
    AutomationTestSupport::TestRuntime fixture;
    auto &runtime = fixture.runtime();
    const auto state =
        runtime.facade().getEditorState(runtime.documentVersion().documentId, *runtime.windowId());
    const auto center = runtime.facade().centerPianoRoll(guiContext(runtime), 120.0, 60.0);
    const auto quantize = runtime.facade().setPianoRollQuantize(guiContext(runtime), 16, true);
    QVERIFY2((state && !state.get().view),
             qPrintable(QStringLiteral("editor state remains queryable without an attached view")));
    QVERIFY2(
        (!center &&
         center.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         center.getError().operationId == Automation::OperationIds::editor::center_piano_roll),
        qPrintable(QStringLiteral("missing editor view host must be explicit")));
    QVERIFY2(
        (!quantize &&
         quantize.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         quantize.getError().operationId == Automation::OperationIds::editor::set_quantize),
        qPrintable(QStringLiteral("missing stable editor host must be explicit")));
}

void TestAutomationApplicationDomains::unavailableSettingsHost() {
    AutomationTestSupport::TestRuntime fixture;
    auto &runtime = fixture.runtime();
    const auto settings = runtime.settings().getSettings();
    const auto updateGeneral =
        runtime.settings().updateGeneral(applicationContext(), validSettings().general);
    const auto recent = runtime.settings().getRecentProjectFiles();
    QVERIFY2(
        (!settings &&
         settings.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         settings.getError().operationId == Automation::OperationIds::settings::query),
        qPrintable(QStringLiteral("missing settings snapshot host must be explicit")));
    QVERIFY2((!updateGeneral &&
              updateGeneral.getError().code ==
                  Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              updateGeneral.getError().operationId ==
                  Automation::OperationIds::settings::update_general),
             qPrintable(QStringLiteral("missing settings persistence host must be explicit")));
    QVERIFY2(
        (!recent &&
         recent.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         recent.getError().operationId == Automation::OperationIds::recent_files::list),
        qPrintable(QStringLiteral("missing recent-file host must be explicit")));
}

void TestAutomationApplicationDomains::unavailablePackagesHost() {
    AutomationTestSupport::TestRuntime fixture;
    auto &runtime = fixture.runtime();
    const auto packages = runtime.packages().getInstalledPackages();
    const auto validation = runtime.packages().validatePackage(QStringLiteral("package.dspk"));
    const auto resolve = runtime.packages().resolveDocumentVoices(commandContext(runtime));
    QVERIFY2((!packages &&
              packages.getError().code == Automation::AutomationErrorCode::ModuleNotReady &&
              packages.getError().operationId == Automation::OperationIds::packages::list),
             qPrintable(QStringLiteral("missing package registry must be explicit")));
    QVERIFY2((!validation &&
              validation.getError().code == Automation::AutomationErrorCode::ModuleNotReady &&
              validation.getError().operationId == Automation::OperationIds::packages::validate),
             qPrintable(QStringLiteral("missing package validator must be explicit")));
    QVERIFY2((!resolve &&
              resolve.getError().code == Automation::AutomationErrorCode::ModuleNotReady &&
              resolve.getError().operationId ==
                  Automation::OperationIds::packages::resolve_document_voices),
             qPrintable(QStringLiteral("missing voice resolver must be explicit")));
}

void TestAutomationApplicationDomains::unavailablePresetsHost() {
    AutomationTestSupport::TestRuntime fixture;
    auto &runtime = fixture.runtime();
    const auto presets = runtime.presets().getSpeakerMixPresets();
    const auto save = runtime.presets().saveSpeakerMixPreset(applicationContext(), validPreset());
    const auto remove =
        runtime.presets().deleteSpeakerMixPreset(applicationContext(), QStringLiteral("preset"));
    QVERIFY2(
        (!presets &&
         presets.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         presets.getError().operationId == Automation::OperationIds::speaker_mix_presets::list),
        qPrintable(QStringLiteral("missing preset list host must be explicit")));
    QVERIFY2((!save &&
              save.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              save.getError().operationId == Automation::OperationIds::speaker_mix_presets::save),
             qPrintable(QStringLiteral("missing preset save host must be explicit")));
    QVERIFY2(
        (!remove &&
         remove.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         remove.getError().operationId ==
             Automation::OperationIds::speaker_mix_presets::delete_preset),
        qPrintable(QStringLiteral("missing preset delete host must be explicit")));
}
