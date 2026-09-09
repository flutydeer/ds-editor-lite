#include "TestAutomationApplicationDomains.h"
#include "ApplicationHarness.h"

#include <QtTest>
#include <limits>

using namespace ApplicationTest;

void TestAutomationApplicationDomains::playbackSnapshotAndDocumentIdentity() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto initialVersion = runtime.documentVersion();
    harness.playback.position = 120.0;
    harness.playback.lastPosition = 80.0;
    const auto snapshot = runtime.playback().getPlayback(initialVersion.documentId);
    QVERIFY2((snapshot && snapshot.get().document == initialVersion &&
              snapshot.get().position == 120.0 && snapshot.get().lastPosition == 80.0 &&
              snapshot.get().state == Automation::PlaybackState::Stopped),
             qPrintable(
                 QStringLiteral("playback query must preserve host values and document version")));

    const auto wrongDocument = runtime.playback().getPlayback(Automation::DocumentId::create());
    QVERIFY2(
        (!wrongDocument &&
         wrongDocument.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
         wrongDocument.getError().operationId == Automation::OperationIds::playback::get_state),
        qPrintable(QStringLiteral("playback query must reject another document")));
}

void TestAutomationApplicationDomains::playbackTransitionsAreTransient() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto initialVersion = runtime.documentVersion();
    const auto playPreview = runtime.playback().play(commandContext(runtime, true));
    QVERIFY2((playPreview && playPreview.get().changed && playPreview.get().validatedOnly &&
              harness.playCalls == 0 && runtime.documentVersion() == initialVersion),
             qPrintable(QStringLiteral(
                 "play preview must predict without host or revision side effects")));
    const auto play = runtime.playback().play(commandContext(runtime));
    const auto playNoOp = runtime.playback().play(commandContext(runtime));
    QVERIFY2((play && play.get().changed && playNoOp && !playNoOp.get().changed &&
              harness.playCalls == 1 && runtime.documentVersion() == initialVersion),
             qPrintable(QStringLiteral("play must call once and repeated play must be a no-op")));

    const auto pause = runtime.playback().pause(commandContext(runtime));
    const auto pauseNoOp = runtime.playback().pause(commandContext(runtime));
    const auto stop = runtime.playback().stop(commandContext(runtime));
    const auto stopNoOp = runtime.playback().stop(commandContext(runtime));
    QVERIFY2((pause && pause.get().changed && pauseNoOp && !pauseNoOp.get().changed && stop &&
              stop.get().changed && stopNoOp && !stopNoOp.get().changed &&
              harness.pauseCalls == 1 && harness.stopCalls == 1 &&
              runtime.documentVersion() == initialVersion),
             qPrintable(QStringLiteral("pause and stop must each suppress repeated host calls")));
}

void TestAutomationApplicationDomains::playbackStartFailuresPreserveState() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto initialVersion = runtime.documentVersion();
    harness.playbackCanStart = false;
    const auto busy = runtime.playback().play(commandContext(runtime));
    QVERIFY2((!busy && busy.getError().code == Automation::AutomationErrorCode::Busy &&
              busy.getError().operationId == Automation::OperationIds::playback::play),
             qPrintable(QStringLiteral("an active editor gesture must block playback start")));
    QVERIFY2((harness.playCalls == 0),
             qPrintable(QStringLiteral("busy playback must not call the device")));

    harness.playbackCanStart = true;
    harness.playbackPlaySucceeds = false;
    const auto failedStart = runtime.playback().play(commandContext(runtime));
    QVERIFY2(
        (!failedStart &&
         failedStart.getError().code ==
             Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         failedStart.getError().operationId == Automation::OperationIds::playback::play),
        qPrintable(QStringLiteral("device start failure must be reported without state change")));
    QVERIFY2((harness.playback.state == Automation::PlaybackState::Stopped &&
              runtime.documentVersion() == initialVersion),
             qPrintable(QStringLiteral("failed playback start must keep state and revision")));
}

void TestAutomationApplicationDomains::playbackPositionsPreviewAndCommit() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto initialVersion = runtime.documentVersion();
    const auto positionPreview =
        runtime.playback().setPosition(commandContext(runtime, true), 960.0);
    const auto position = runtime.playback().setPosition(commandContext(runtime), 960.0);
    const auto positionNoOp = runtime.playback().setPosition(commandContext(runtime), 960.0);
    QVERIFY2((positionPreview && positionPreview.get().validatedOnly &&
              positionPreview.get().changed && position && position.get().changed && positionNoOp &&
              !positionNoOp.get().changed && harness.playback.position == 960.0 &&
              harness.positionCalls == 1 && runtime.documentVersion() == initialVersion),
             qPrintable(QStringLiteral("position must support preview, commit and no-op")));

    const auto negativePosition = runtime.playback().setPosition(commandContext(runtime), -1.0);
    const auto nanPosition = runtime.playback().setPosition(
        commandContext(runtime), std::numeric_limits<double>::quiet_NaN());
    const auto infinitePosition = runtime.playback().setPosition(
        commandContext(runtime), std::numeric_limits<double>::infinity());
    QVERIFY2(
        (!negativePosition &&
         negativePosition.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         negativePosition.getError().operationId ==
             Automation::OperationIds::playback::set_position),
        qPrintable(QStringLiteral("negative playback position must be rejected")));
    QVERIFY2(
        (!nanPosition &&
         nanPosition.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         nanPosition.getError().operationId == Automation::OperationIds::playback::set_position),
        qPrintable(QStringLiteral("NaN playback position must be rejected")));
    QVERIFY2(
        (!infinitePosition &&
         infinitePosition.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         infinitePosition.getError().operationId ==
             Automation::OperationIds::playback::set_position),
        qPrintable(QStringLiteral("infinite playback position must be rejected")));

    const auto lastPreview =
        runtime.playback().setLastPosition(commandContext(runtime, true), 480.0);
    const auto last = runtime.playback().setLastPosition(commandContext(runtime), 480.0);
    const auto lastNoOp = runtime.playback().setLastPosition(commandContext(runtime), 480.0);
    const auto invalidLast = runtime.playback().setLastPosition(commandContext(runtime), -0.01);
    QVERIFY2((lastPreview && lastPreview.get().validatedOnly && last && last.get().changed &&
              lastNoOp && !lastNoOp.get().changed && harness.lastPositionCalls == 1 &&
              harness.playback.lastPosition == 480.0),
             qPrintable(QStringLiteral("last position must support preview, commit and no-op")));
    QVERIFY2((!invalidLast &&
              invalidLast.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              invalidLast.getError().operationId ==
                  Automation::OperationIds::playback::set_last_position),
             qPrintable(QStringLiteral("negative last position must be rejected")));
}

void TestAutomationApplicationDomains::playbackLoopMutationsAndValidation() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto loopBase = runtime.documentVersion();
    const LoopSettings range(false, 480, 960);
    const auto loopPreview = runtime.playback().setLoop(commandContext(runtime, true), range);
    const auto loopSet = runtime.playback().setLoop(commandContext(runtime), range);
    const auto loopNoOp = runtime.playback().setLoop(commandContext(runtime), range);
    QVERIFY2((loopPreview && loopPreview.get().validatedOnly && loopPreview.get().changed &&
              loopPreview.get().current.revision == loopBase.revision + 1 && loopSet &&
              loopSet.get().changed && loopNoOp && !loopNoOp.get().changed &&
              runtime.documentVersion().revision == loopBase.revision + 1 &&
              harness.loopCalls == 1),
             qPrintable(QStringLiteral("loop range must record one revision and suppress no-op")));

    const auto enable = runtime.playback().setLoopEnabled(commandContext(runtime), true);
    const auto enableNoOp = runtime.playback().setLoopEnabled(commandContext(runtime), true);
    const auto clear = runtime.playback().clearLoop(commandContext(runtime));
    const auto clearNoOp = runtime.playback().clearLoop(commandContext(runtime));
    QVERIFY2((enable && enable.get().changed && enableNoOp && !enableNoOp.get().changed && clear &&
              clear.get().changed && clearNoOp && !clearNoOp.get().changed &&
              harness.playback.loop == LoopSettings() && harness.loopCalls == 3 &&
              runtime.documentVersion().revision == loopBase.revision + 3),
             qPrintable(QStringLiteral("enable and clear must each commit at most once")));

    const auto enableEmpty = runtime.playback().setLoopEnabled(commandContext(runtime), true);
    const auto zeroEnabled =
        runtime.playback().setLoop(commandContext(runtime), LoopSettings(true, 0, 0));
    const auto negativeRange =
        runtime.playback().setLoop(commandContext(runtime), LoopSettings(false, -1, 20));
    QVERIFY2((!enableEmpty &&
              enableEmpty.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              enableEmpty.getError().operationId ==
                  Automation::OperationIds::playback::set_loop_enabled),
             qPrintable(QStringLiteral("empty loop cannot be enabled")));
    QVERIFY2((!zeroEnabled &&
              zeroEnabled.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              zeroEnabled.getError().operationId == Automation::OperationIds::playback::set_loop),
             qPrintable(QStringLiteral("enabled zero-length loop must be rejected")));
    QVERIFY2((!negativeRange &&
              negativeRange.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              negativeRange.getError().operationId == Automation::OperationIds::playback::set_loop),
             qPrintable(QStringLiteral("negative loop range must be rejected")));
}

void TestAutomationApplicationDomains::playbackCommandGuards() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    auto stale = commandContext(runtime);
    ++stale.expected.revision;
    const auto staleInvalid = runtime.playback().setLoop(stale, LoopSettings(true, 0, 0));
    auto replaced = stale;
    replaced.expected.documentId = Automation::DocumentId::create();
    const auto replacedInvalid = runtime.playback().setLoop(replaced, LoopSettings(true, 0, 0));
    QVERIFY2((!staleInvalid &&
              staleInvalid.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
              staleInvalid.getError().operationId == Automation::OperationIds::playback::set_loop),
             qPrintable(QStringLiteral("revision must be checked before loop validation")));
    QVERIFY2(
        (!replacedInvalid &&
         replacedInvalid.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
         replacedInvalid.getError().operationId == Automation::OperationIds::playback::set_loop),
        qPrintable(QStringLiteral("document must be checked before revision and loop")));

    auto keyed = commandContext(runtime);
    keyed.idempotencyKey = QStringLiteral("playback-state-key");
    const auto unsupportedKey = runtime.playback().pause(keyed);
    QVERIFY2(
        (!unsupportedKey &&
         unsupportedKey.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         unsupportedKey.getError().operationId == Automation::OperationIds::playback::pause),
        qPrintable(QStringLiteral("ephemeral playback state must reject document idempotency")));
}

void TestAutomationApplicationDomains::playbackControlsDuringDocumentWorkflow() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    auto publicControl = commandContext(runtime);
    publicControl.source = Automation::InvocationSource::PublicMcp;
    runtime.setDocumentBusy(publicControl.expected.documentId, true);
    harness.playback.state = Automation::PlaybackState::Playing;
    const auto pauseWhileBusy = runtime.playback().pause(publicControl);
    const auto seekWhileBusy = runtime.playback().seek(publicControl, 1440.0);
    const auto loopWhileBusy =
        runtime.playback().setLoop(publicControl, LoopSettings(false, 480, 960));
    ++publicControl.expected.revision;
    const auto staleSeekWhileBusy = runtime.playback().seek(publicControl, 1920.0);
    runtime.setDocumentBusy(publicControl.expected.documentId, false);
    QVERIFY2(
        (pauseWhileBusy && pauseWhileBusy.get().changed &&
         harness.playback.state == Automation::PlaybackState::Paused && harness.pauseCalls == 1 &&
         seekWhileBusy && seekWhileBusy.get().changed && harness.playback.position == 1440.0 &&
         harness.playback.lastPosition == 1440.0),
        qPrintable(QStringLiteral("workflow busy must keep transient playback control available")));
    QVERIFY2((!loopWhileBusy &&
              loopWhileBusy.getError().code == Automation::AutomationErrorCode::Busy &&
              loopWhileBusy.getError().operationId == Automation::OperationIds::playback::set_loop),
             qPrintable(QStringLiteral("workflow busy must reject persistent playback changes")));
    QVERIFY2(
        (!staleSeekWhileBusy &&
         staleSeekWhileBusy.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
         staleSeekWhileBusy.getError().operationId == Automation::OperationIds::playback::seek),
        qPrintable(QStringLiteral("transient playback control must still validate revision")));
}
