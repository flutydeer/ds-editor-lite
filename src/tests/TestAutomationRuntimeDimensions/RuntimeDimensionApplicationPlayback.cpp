#include "RuntimeDimensionSupport.h"

#include <limits>

namespace RuntimeDimensions {
    namespace {
        using MutationResult = Automation::AutomationResult<Automation::MutationResult>;

        void runApplicationInfo(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::application::get_info);

            log.run(operationId, QStringLiteral("NORMAL"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().application().getInfo();
                log.expect(result && result.get() == harness.applicationInfo &&
                               harness.hostCalls.value(operationId) == 1,
                           QStringLiteral("application info must return the typed host snapshot"));
            });
            log.run(operationId, QStringLiteral("UNICODE"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().application().getInfo();
                log.expect(result && result.get().name.contains(QStringLiteral("测试")) &&
                               result.get().platform.contains(QStringLiteral("测试")),
                           QStringLiteral("application info must preserve Unicode fields"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().application().getInfo();
                const auto captured = result ? result.get() : Automation::ApplicationInfoDto{};
                harness.applicationInfo.name = QStringLiteral("changed after query");
                log.expect(result && result.get() == captured &&
                               result.get().name != harness.applicationInfo.name,
                           QStringLiteral("application info must be detached from host storage"));
            });
            log.run(operationId, QStringLiteral("REPEATED-QUERY"), [&] {
                RuntimeHarness harness;
                const auto first = harness.core().application().getInfo();
                const auto second = harness.core().application().getInfo();
                log.expect(first && second && first.get() == second.get() &&
                               harness.hostCalls.value(operationId) == 2,
                           QStringLiteral("repeated info queries must be deterministic"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().application().getInfo();
                log.expect(result && harness.core().documentVersion() == version &&
                               harness.settingsWrites == 0 && harness.presetWrites == 0,
                           QStringLiteral("application info must not mutate document or storage"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().application().getInfo();
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing info host must be operation-decorated"));
            });
        }

        void runTermination(ScenarioLog &log, const Automation::ApplicationTerminationMode mode,
                            const Automation::OperationId &operationId) {
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().application().requestTermination(
                    guiContext(harness, true), mode);
                log.expect(result && result.get().changed && result.get().validatedOnly &&
                               result.get().windowId == harness.core().windowId() &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("termination preview must not invoke the host"));
            });
            log.run(operationId, QStringLiteral("COMMIT"), [&] {
                RuntimeHarness harness;
                auto context = guiContext(harness);
                context.source = Automation::InvocationSource::TrustedGui;
                const auto result = harness.core().application().requestTermination(context, mode);
                log.expect(
                    result && result.get().changed && !result.get().validatedOnly &&
                        harness.lastTerminationMode == mode &&
                        harness.lastTerminationSavePolicy ==
                            Automation::ApplicationTerminationSavePolicy::Prompt &&
                        harness.hostCalls.value(operationId) == 1,
                    QStringLiteral("termination commit must forward the selected mode once"));
            });
            log.run(operationId, QStringLiteral("PUBLIC-REJECT-UNSAVED-POLICY"), [&] {
                RuntimeHarness harness;
                auto context = guiContext(harness);
                context.source = Automation::InvocationSource::PublicMcp;
                context.clientId = QStringLiteral("runtime-public");
                const auto result =
                    harness.core().application().requestTermination(context, mode);
                log.expect(
                    result && harness.lastTerminationSavePolicy ==
                                  Automation::ApplicationTerminationSavePolicy::RejectUnsaved,
                    QStringLiteral("public lifecycle requests must default to rejecting unsaved "
                                   "changes without prompting"));
            });
            log.run(operationId, QStringLiteral("PUBLIC-DISCARD-POLICY"), [&] {
                RuntimeHarness harness;
                auto context = guiContext(harness);
                context.source = Automation::InvocationSource::PublicMcp;
                context.clientId = QStringLiteral("runtime-public");
                const auto result =
                    harness.core().application().requestTermination(context, mode, true);
                log.expect(result && harness.lastTerminationSavePolicy ==
                                         Automation::ApplicationTerminationSavePolicy::Discard,
                           QStringLiteral("discard_changes must select the non-interactive discard "
                                          "policy"));
            });
            log.run(operationId, QStringLiteral("SINGLE-HOST-CALL"), [&] {
                RuntimeHarness harness;
                const auto result =
                    harness.core().application().requestTermination(guiContext(harness), mode);
                log.expect(
                    result && harness.hostCalls.value(operationId) == 1,
                    QStringLiteral("one accepted request must produce exactly one host call"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-WINDOW"), [&] {
                RuntimeHarness harness;
                auto context = guiContext(harness);
                context.windowId = Automation::WindowId::create();
                const auto result = harness.core().application().requestTermination(context, mode);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("unknown window must fail before termination host"));
                log.expect(harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("unknown window must not reach termination host"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result =
                    harness.core().application().requestTermination(guiContext(harness), mode);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing termination host must be explicit"));
            });
            log.run(operationId, QStringLiteral("HOST-REJECT"), [&] {
                RuntimeHarness harness;
                harness.applicationTerminationSucceeds = false;
                const auto result =
                    harness.core().application().requestTermination(guiContext(harness), mode);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("termination rejection must remain atomic"));
                log.expect(harness.hostCalls.value(operationId) == 1,
                           QStringLiteral("rejected request must still call its host only once"));
            });
            log.run(operationId, QStringLiteral("REPEATED-REQUEST-POLICY"), [&] {
                RuntimeHarness harness;
                const auto first =
                    harness.core().application().requestTermination(guiContext(harness), mode);
                const auto second =
                    harness.core().application().requestTermination(guiContext(harness), mode);
                log.expect(
                    first && second && harness.hostCalls.value(operationId) == 2,
                    QStringLiteral("unkeyed lifecycle requests must each be forwarded once"));
            });
        }

        void runPlaybackQuery(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::playback::get);
            log.run(operationId, QStringLiteral("MINIMAL"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().playback().getPlayback(
                    harness.core().documentVersion().documentId);
                log.expect(result && result.get().document == harness.core().documentVersion() &&
                               result.get().state == Automation::PlaybackState::Stopped,
                           QStringLiteral("playback query must expose the active document"));
            });
            log.run(operationId, QStringLiteral("RICH-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                harness.playback.state = Automation::PlaybackState::Paused;
                harness.playback.position = 123.5;
                harness.playback.lastPosition = 99.25;
                harness.playback.loop = LoopSettings(true, 480, 960);
                const auto result = harness.core().playback().getPlayback(
                    harness.core().documentVersion().documentId);
                log.expect(result && result.get().state == Automation::PlaybackState::Paused &&
                               result.get().position == 123.5 &&
                               result.get().lastPosition == 99.25 &&
                               result.get().loop == LoopSettings(true, 480, 960),
                           QStringLiteral("playback query must preserve all host fields"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                harness.playback.position = 42.0;
                const auto result = harness.core().playback().getPlayback(
                    harness.core().documentVersion().documentId);
                harness.playback.position = 84.0;
                log.expect(result && result.get().position == 42.0,
                           QStringLiteral("playback query result must be detached from the host"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto before = harness.core().documentVersion();
                const auto hostBefore = harness.playback;
                const auto result = harness.core().playback().getPlayback(before.documentId);
                log.expect(result && harness.core().documentVersion() == before &&
                               harness.playback.state == hostBefore.state &&
                               harness.playback.position == hostBefore.position &&
                               harness.hostCalls.value(QStringLiteral("playback.loop.apply")) == 0,
                           QStringLiteral("playback query must not mutate host or document"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                const auto result =
                    harness.core().playback().getPlayback(Automation::DocumentId::create());
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("playback query must route by explicit document"));
                log.expect(harness.hostCalls.value(QStringLiteral("playback.snapshot")) == 0,
                           QStringLiteral("unknown document must not query playback host"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().playback().getPlayback(
                    harness.core().documentVersion().documentId);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing playback snapshot must be explicit"));
            });
        }

        MutationResult invokeState(RuntimeHarness &harness,
                                   const Automation::OperationId &operationId,
                                   const Automation::CommandContext &context) {
            if (operationId == Automation::OperationIds::playback::play)
                return harness.core().playback().play(context);
            if (operationId == Automation::OperationIds::playback::pause)
                return harness.core().playback().pause(context);
            return harness.core().playback().stop(context);
        }

        Automation::PlaybackState targetState(const Automation::OperationId &operationId) {
            if (operationId == Automation::OperationIds::playback::play)
                return Automation::PlaybackState::Playing;
            if (operationId == Automation::OperationIds::playback::pause)
                return Automation::PlaybackState::Paused;
            return Automation::PlaybackState::Stopped;
        }

        void runPlaybackStateCommand(ScenarioLog &log, const Automation::OperationId &operationId) {
            const auto target = targetState(operationId);
            const auto initial = target == Automation::PlaybackState::Stopped
                                     ? Automation::PlaybackState::Playing
                                     : Automation::PlaybackState::Stopped;
            log.run(operationId, QStringLiteral("NORMAL"), [&] {
                RuntimeHarness harness;
                harness.playback.state = initial;
                const auto version = harness.core().documentVersion();
                const auto result = invokeState(harness, operationId, commandContext(harness));
                log.expect(
                    result && result.get().changed && harness.playback.state == target &&
                        harness.hostCalls.value(operationId) == 1 &&
                        harness.core().documentVersion() == version,
                    QStringLiteral("playback state transition must call once without revision"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                harness.playback.state = target;
                const auto result = invokeState(harness, operationId, commandContext(harness));
                log.expect(result && !result.get().changed &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("repeated playback state must be a host-free no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                harness.playback.state = initial;
                const auto result =
                    invokeState(harness, operationId, commandContext(harness, true));
                log.expect(result && result.get().changed && result.get().validatedOnly &&
                               harness.playback.state == initial &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("playback state preview must not call its apply host"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                harness.playback.state = initial;
                auto context = commandContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                const auto result = invokeState(harness, operationId, context);
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("playback state must reject another document"));
                log.expect(
                    harness.hostCalls.value(operationId) == 0,
                    QStringLiteral("document routing failure must not invoke playback host"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                harness.playback.state = initial;
                auto context = commandContext(harness);
                ++context.expected.revision;
                const auto result = invokeState(harness, operationId, context);
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId,
                                QStringLiteral("playback state must reject stale revision"));
            });
            if (operationId == Automation::OperationIds::playback::play) {
                log.run(operationId, QStringLiteral("BUSY"), [&] {
                    RuntimeHarness harness;
                    harness.playbackCanStart = false;
                    const auto result = harness.core().playback().play(commandContext(harness));
                    log.expectError(result, Automation::AutomationErrorCode::Busy, operationId,
                                    QStringLiteral("busy editor gesture must block playback"));
                    log.expect(harness.hostCalls.value(operationId) == 0,
                               QStringLiteral("busy playback must not start the device"));
                });
                log.run(operationId, QStringLiteral("DEVICE-FAILURE"), [&] {
                    RuntimeHarness harness;
                    harness.playbackPlaySucceeds = false;
                    const auto result = harness.core().playback().play(commandContext(harness));
                    log.expectError(
                        result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        operationId, QStringLiteral("device start failure must remain atomic"));
                    log.expect(harness.playback.state == Automation::PlaybackState::Stopped &&
                                   harness.hostCalls.value(operationId) == 1,
                               QStringLiteral("failed device start must not change state"));
                });
            }
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                harness.playback.state = initial;
                const auto result = invokeState(harness, operationId, commandContext(harness));
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing playback callback must be explicit"));
            });
            log.run(operationId, QStringLiteral("IDEMPOTENCY-REJECTED"), [&] {
                RuntimeHarness harness;
                harness.playback.state = initial;
                const auto result = invokeState(
                    harness, operationId,
                    commandContext(harness, false, QStringLiteral("ephemeral-playback-key")));
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("ephemeral playback command must reject keys"));
                log.expect(harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("rejected key must not invoke playback host"));
            });
        }

        MutationResult invokePosition(RuntimeHarness &harness,
                                      const Automation::OperationId &operationId,
                                      const Automation::CommandContext &context,
                                      const double tick) {
            if (operationId == Automation::OperationIds::playback::set_position)
                return harness.core().playback().setPosition(context, tick);
            return harness.core().playback().setLastPosition(context, tick);
        }

        double positionValue(const RuntimeHarness &harness,
                             const Automation::OperationId &operationId) {
            return operationId == Automation::OperationIds::playback::set_position
                       ? harness.playback.position
                       : harness.playback.lastPosition;
        }

        void runPlaybackPositionCommand(ScenarioLog &log,
                                        const Automation::OperationId &operationId) {
            log.run(operationId, QStringLiteral("NORMAL"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result =
                    invokePosition(harness, operationId, commandContext(harness), 1234.5);
                log.expect(result && result.get().changed &&
                               positionValue(harness, operationId) == 1234.5 &&
                               harness.hostCalls.value(operationId) == 1 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("position update must apply once without revision"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                if (operationId == Automation::OperationIds::playback::set_position)
                    harness.playback.position = 80.0;
                else
                    harness.playback.lastPosition = 80.0;
                const auto result =
                    invokePosition(harness, operationId, commandContext(harness), 80.0);
                log.expect(result && !result.get().changed &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("identical position must be a host-free no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto result =
                    invokePosition(harness, operationId, commandContext(harness, true), 64.0);
                log.expect(result && result.get().changed && result.get().validatedOnly &&
                               positionValue(harness, operationId) == 0.0 &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("position preview must not mutate the host"));
            });
            log.run(operationId, QStringLiteral("NEGATIVE"), [&] {
                RuntimeHarness harness;
                const auto result =
                    invokePosition(harness, operationId, commandContext(harness), -0.01);
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId, QStringLiteral("negative position must be rejected"));
            });
            log.run(operationId, QStringLiteral("NON-FINITE"), [&] {
                RuntimeHarness harness;
                const auto nan = invokePosition(harness, operationId, commandContext(harness),
                                                std::numeric_limits<double>::quiet_NaN());
                const auto infinity = invokePosition(harness, operationId, commandContext(harness),
                                                     std::numeric_limits<double>::infinity());
                log.expectError(nan, Automation::AutomationErrorCode::InvalidArgument, operationId,
                                QStringLiteral("NaN position must be rejected"));
                log.expectError(infinity, Automation::AutomationErrorCode::InvalidArgument,
                                operationId, QStringLiteral("infinite position must be rejected"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                const auto result = invokePosition(harness, operationId, context, -1.0);
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("document routing must precede tick validation"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                ++context.expected.revision;
                const auto result = invokePosition(harness, operationId, context, -1.0);
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId,
                                QStringLiteral("revision routing must precede tick validation"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result =
                    invokePosition(harness, operationId, commandContext(harness), 42.0);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing position callback must be explicit"));
            });
            log.run(operationId, QStringLiteral("IDEMPOTENCY-REJECTED"), [&] {
                RuntimeHarness harness;
                const auto result = invokePosition(
                    harness, operationId,
                    commandContext(harness, false, QStringLiteral("position-key")), 12.0);
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("ephemeral position must reject idempotency key"));
            });
        }

        void runSetLoop(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::playback::set_loop);
            const LoopSettings target(false, 480, 960);
            log.run(operationId, QStringLiteral("NORMAL-HISTORY"), [&] {
                RuntimeHarness harness;
                const auto before = harness.core().documentVersion();
                const auto result =
                    harness.core().playback().setLoop(commandContext(harness), target);
                const auto history =
                    harness.core().history().getState(harness.core().documentVersion().documentId);
                log.expect(
                    result && result.get().changed &&
                        result.get().current.revision == before.revision + 1 && history &&
                        history.get().canUndo && harness.playback.loop == target &&
                        harness.hostCalls.value(QStringLiteral("playback.loop.apply")) == 1,
                    QStringLiteral("loop range must commit one history action and revision"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                harness.playback.loop = target;
                const auto before = harness.core().documentVersion();
                const auto result =
                    harness.core().playback().setLoop(commandContext(harness), target);
                log.expect(result && !result.get().changed &&
                               harness.core().documentVersion() == before &&
                               harness.hostCalls.value(QStringLiteral("playback.loop.apply")) == 0,
                           QStringLiteral("identical loop range must not add history or revision"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto before = harness.core().documentVersion();
                const auto result =
                    harness.core().playback().setLoop(commandContext(harness, true), target);
                log.expect(
                    result && result.get().validatedOnly && result.get().changed &&
                        result.get().current.revision == before.revision + 1 &&
                        harness.core().documentVersion() == before &&
                        harness.playback.loop == LoopSettings(),
                    QStringLiteral("loop preview must predict without applying or recording"));
            });
            log.run(operationId, QStringLiteral("INVALID-RANGE"), [&] {
                RuntimeHarness harness;
                const auto negative = harness.core().playback().setLoop(
                    commandContext(harness), LoopSettings(false, -1, 10));
                const auto emptyEnabled = harness.core().playback().setLoop(
                    commandContext(harness), LoopSettings(true, 0, 0));
                log.expectError(negative, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("negative loop start must be rejected"));
                log.expectError(emptyEnabled, Automation::AutomationErrorCode::InvalidArgument,
                                operationId, QStringLiteral("enabled empty loop must be rejected"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                const auto result =
                    harness.core().playback().setLoop(context, LoopSettings(true, 0, 0));
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("document must precede loop validation"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                ++context.expected.revision;
                const auto result =
                    harness.core().playback().setLoop(context, LoopSettings(true, 0, 0));
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId,
                                QStringLiteral("revision must precede loop validation"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result =
                    harness.core().playback().setLoop(commandContext(harness), target);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing loop apply host must be explicit"));
            });
            log.run(operationId, QStringLiteral("IDEMPOTENCY-REJECTED"), [&] {
                RuntimeHarness harness;
                const auto context = commandContext(harness, false, QStringLiteral("set-loop-key"));
                const auto result = harness.core().playback().setLoop(context, target);
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("loop mutation must reject idempotency keys"));
            });
        }

        void runSetLoopEnabled(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::playback::set_loop_enabled);
            log.run(operationId, QStringLiteral("NORMAL-HISTORY"), [&] {
                RuntimeHarness harness;
                harness.playback.loop = LoopSettings(false, 100, 500);
                const auto before = harness.core().documentVersion();
                const auto result =
                    harness.core().playback().setLoopEnabled(commandContext(harness), true);
                log.expect(result && result.get().changed && harness.playback.loop.enabled &&
                               harness.core().documentVersion().revision == before.revision + 1 &&
                               harness.hostCalls.value(QStringLiteral("playback.loop.apply")) == 1,
                           QStringLiteral("loop enable must commit one persistent mutation"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                harness.playback.loop = LoopSettings(true, 100, 500);
                const auto before = harness.core().documentVersion();
                const auto result =
                    harness.core().playback().setLoopEnabled(commandContext(harness), true);
                log.expect(result && !result.get().changed &&
                               harness.core().documentVersion() == before &&
                               harness.hostCalls.value(QStringLiteral("playback.loop.apply")) == 0,
                           QStringLiteral("repeated loop enable must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                harness.playback.loop = LoopSettings(false, 100, 500);
                const auto before = harness.core().documentVersion();
                const auto result =
                    harness.core().playback().setLoopEnabled(commandContext(harness, true), true);
                log.expect(result && result.get().validatedOnly && result.get().changed &&
                               !harness.playback.loop.enabled &&
                               harness.core().documentVersion() == before,
                           QStringLiteral("loop enable preview must not mutate host or revision"));
            });
            log.run(operationId, QStringLiteral("EMPTY-RANGE"), [&] {
                RuntimeHarness harness;
                const auto result =
                    harness.core().playback().setLoopEnabled(commandContext(harness), true);
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId, QStringLiteral("empty loop cannot be enabled"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                const auto result = harness.core().playback().setLoopEnabled(context, true);
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("loop enable must route explicit document"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                ++context.expected.revision;
                const auto result = harness.core().playback().setLoopEnabled(context, true);
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId,
                                QStringLiteral("loop enable must check revision first"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                harness.playback.loop = LoopSettings(false, 100, 500);
                const auto result =
                    harness.core().playback().setLoopEnabled(commandContext(harness), true);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing loop host must reject enable"));
            });
            log.run(operationId, QStringLiteral("IDEMPOTENCY-REJECTED"), [&] {
                RuntimeHarness harness;
                harness.playback.loop = LoopSettings(false, 100, 500);
                const auto context =
                    commandContext(harness, false, QStringLiteral("enable-loop-key"));
                const auto result = harness.core().playback().setLoopEnabled(context, true);
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("loop enable must reject idempotency keys"));
            });
        }

        void runClearLoop(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::playback::clear_loop);
            log.run(operationId, QStringLiteral("NORMAL-HISTORY"), [&] {
                RuntimeHarness harness;
                harness.playback.loop = LoopSettings(true, 100, 500);
                const auto result = harness.core().playback().clearLoop(commandContext(harness));
                log.expect(result && result.get().changed &&
                               harness.playback.loop == LoopSettings() &&
                               harness.core().documentVersion().revision == 1 &&
                               harness.hostCalls.value(QStringLiteral("playback.loop.apply")) == 1,
                           QStringLiteral("clear loop must commit one history mutation"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().playback().clearLoop(commandContext(harness));
                log.expect(result && !result.get().changed &&
                               harness.core().documentVersion().revision == 0 &&
                               harness.hostCalls.value(QStringLiteral("playback.loop.apply")) == 0,
                           QStringLiteral("clearing an empty loop must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                harness.playback.loop = LoopSettings(true, 100, 500);
                const auto before = harness.core().documentVersion();
                const auto result =
                    harness.core().playback().clearLoop(commandContext(harness, true));
                log.expect(result && result.get().validatedOnly && result.get().changed &&
                               harness.playback.loop.enabled &&
                               harness.core().documentVersion() == before,
                           QStringLiteral("clear preview must not change loop or revision"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                const auto result = harness.core().playback().clearLoop(context);
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("clear loop must reject another document"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                auto context = commandContext(harness);
                ++context.expected.revision;
                const auto result = harness.core().playback().clearLoop(context);
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId,
                                QStringLiteral("clear loop must reject stale revision"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                harness.playback.loop = LoopSettings(true, 100, 500);
                const auto result = harness.core().playback().clearLoop(commandContext(harness));
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId, QStringLiteral("missing loop host must reject clear"));
            });
            log.run(operationId, QStringLiteral("IDEMPOTENCY-REJECTED"), [&] {
                RuntimeHarness harness;
                harness.playback.loop = LoopSettings(true, 100, 500);
                const auto context =
                    commandContext(harness, false, QStringLiteral("clear-loop-key"));
                const auto result = harness.core().playback().clearLoop(context);
                log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("clear loop must reject idempotency keys"));
            });
        }
    }

    void runApplicationPlaybackDimensions(ScenarioLog &log) {
        runApplicationInfo(log);
        runTermination(log, Automation::ApplicationTerminationMode::Exit,
                       Automation::OperationIds::application::request_exit);
        runTermination(log, Automation::ApplicationTerminationMode::Restart,
                       Automation::OperationIds::application::request_restart);
        runPlaybackQuery(log);
        runPlaybackStateCommand(log, Automation::OperationIds::playback::play);
        runPlaybackStateCommand(log, Automation::OperationIds::playback::pause);
        runPlaybackStateCommand(log, Automation::OperationIds::playback::stop);
        runPlaybackPositionCommand(log, Automation::OperationIds::playback::set_position);
        runPlaybackPositionCommand(log, Automation::OperationIds::playback::set_last_position);
        runSetLoop(log);
        runSetLoopEnabled(log);
        runClearLoop(log);
    }

} // namespace RuntimeDimensions
