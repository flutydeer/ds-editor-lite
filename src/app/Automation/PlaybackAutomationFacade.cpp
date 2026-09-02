#include "PlaybackAutomationFacade.h"
#include "OperationIds.h"

#include <lite/History/ActionSequence.h>

#include <cmath>
#include <memory>
#include <utility>

namespace Automation {
    namespace {
        AutomationError unavailable(const QString &message) {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.message = message;
            return error;
        }

        MutationResult hostMutation(DocumentSession &session, const bool changed,
                                    const bool validateOnly) {
            MutationResult result;
            result.previous = session.version();
            result.current = result.previous;
            result.changed = changed;
            result.validatedOnly = validateOnly;
            return result;
        }

        class SetLoopSettingsAction final : public IAction {
        public:
            SetLoopSettingsAction(LoopSettings previous, LoopSettings current,
                                  std::function<void(const LoopSettings &)> apply)
                : m_previous(std::move(previous)), m_current(std::move(current)),
                  m_apply(std::move(apply)) {
            }

            void execute() override {
                m_apply(m_current);
            }

            void undo() override {
                m_apply(m_previous);
            }

        private:
            LoopSettings m_previous;
            LoopSettings m_current;
            std::function<void(const LoopSettings &)> m_apply;
        };

        class SetLoopSettingsActions final : public ActionSequence {
        public:
            SetLoopSettingsActions(LoopSettings previous, LoopSettings current,
                                   std::function<void(const LoopSettings &)> apply) {
                setTranslatableName("SetLoopSettingsActions",
                                    QT_TRANSLATE_NOOP("SetLoopSettingsActions", "Edit loop"));
                addAction(new SetLoopSettingsAction(std::move(previous), std::move(current),
                                                    std::move(apply)));
            }
        };
    }

    PlaybackAutomationFacade::PlaybackAutomationFacade(AutomationDispatcher &dispatcher,
                                                       CommandCommitter &committer,
                                                       PlaybackRuntimeServices services)
        : m_dispatcher(dispatcher), m_committer(committer), m_services(std::move(services)) {
    }

    AutomationResult<PlaybackSnapshotDto>
        PlaybackAutomationFacade::getPlayback(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<PlaybackSnapshotDto>(
            OperationIds::playback::get_state, documentId, [this](DocumentSession &session) {
                if (!m_services.snapshot)
                    return AutomationResult<PlaybackSnapshotDto>(
                        unavailable(QStringLiteral("Playback host is unavailable")));
                const auto host = m_services.snapshot();
                PlaybackSnapshotDto result;
                result.state = host.state;
                result.position = host.position;
                result.lastPosition = host.lastPosition;
                result.loop = host.loop;
                result.document = session.version();
                result.playable = m_services.canStart && m_services.canStart();
                return AutomationResult<PlaybackSnapshotDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult> PlaybackAutomationFacade::play(const CommandContext &context) {
        return setState(OperationIds::playback::play, context, PlaybackState::Playing);
    }

    AutomationResult<MutationResult>
        PlaybackAutomationFacade::pause(const CommandContext &context) {
        return setState(OperationIds::playback::pause, context, PlaybackState::Paused);
    }

    AutomationResult<MutationResult> PlaybackAutomationFacade::stop(const CommandContext &context) {
        return setState(OperationIds::playback::stop, context, PlaybackState::Stopped);
    }

    AutomationResult<MutationResult> PlaybackAutomationFacade::setState(
        const OperationId &operationId, const CommandContext &context, const PlaybackState state) {
        return m_dispatcher.dispatchDocumentControlCommand(
            operationId, context, [this, state](DocumentSession &session, const bool validateOnly) {
                if (!m_services.snapshot)
                    return AutomationResult<MutationResult>(
                        unavailable(QStringLiteral("Playback host is unavailable")));
                const auto current = m_services.snapshot();
                const bool changed = current.state != state;
                if (changed && state == PlaybackState::Playing && m_services.canStart &&
                    !m_services.canStart()) {
                    AutomationError error;
                    error.code = AutomationErrorCode::Busy;
                    error.message = QStringLiteral("An editor gesture is still in progress");
                    return AutomationResult<MutationResult>(std::move(error));
                }
                if (validateOnly || !changed)
                    return AutomationResult<MutationResult>(
                        hostMutation(session, changed, validateOnly));

                if (state == PlaybackState::Playing) {
                    if (!m_services.play || !m_services.play()) {
                        return AutomationResult<MutationResult>(
                            unavailable(QStringLiteral("Playback device could not be started")));
                    }
                } else if (state == PlaybackState::Paused) {
                    if (!m_services.pause)
                        return AutomationResult<MutationResult>(
                            unavailable(QStringLiteral("Playback host is unavailable")));
                    m_services.pause();
                } else {
                    if (!m_services.stop)
                        return AutomationResult<MutationResult>(
                            unavailable(QStringLiteral("Playback host is unavailable")));
                    m_services.stop();
                }
                return AutomationResult<MutationResult>(hostMutation(session, true, false));
            });
    }

    AutomationResult<MutationResult>
        PlaybackAutomationFacade::setPosition(const CommandContext &context, const double tick) {
        return m_dispatcher.dispatchDocumentControlCommand(
            OperationIds::playback::set_position, context,
            [this, tick](DocumentSession &session, const bool validateOnly) {
                if (!std::isfinite(tick) || tick < 0.0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("tick"), QStringLiteral("Playback position is invalid")));
                }
                if (!m_services.snapshot || !m_services.setPosition)
                    return AutomationResult<MutationResult>(
                        unavailable(QStringLiteral("Playback host is unavailable")));
                const bool changed = m_services.snapshot().position != tick;
                if (!validateOnly && changed)
                    m_services.setPosition(tick);
                return AutomationResult<MutationResult>(
                    hostMutation(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult> PlaybackAutomationFacade::seek(const CommandContext &context,
                                                                    const double tick) {
        return m_dispatcher.dispatchDocumentControlCommand(
            OperationIds::playback::seek, context,
            [this, tick](DocumentSession &session, const bool validateOnly) {
                if (!std::isfinite(tick) || tick < 0.0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("position"),
                        QStringLiteral("Playback position is invalid")));
                }
                if (!m_services.snapshot || !m_services.setPosition ||
                    !m_services.setLastPosition) {
                    return AutomationResult<MutationResult>(
                        unavailable(QStringLiteral("Playback host is unavailable")));
                }
                const auto current = m_services.snapshot();
                const bool changed = current.position != tick || current.lastPosition != tick;
                if (!validateOnly && changed) {
                    m_services.setPosition(tick);
                    m_services.setLastPosition(tick);
                }
                return AutomationResult<MutationResult>(
                    hostMutation(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult>
        PlaybackAutomationFacade::setLastPosition(const CommandContext &context,
                                                  const double tick) {
        return m_dispatcher.dispatchDocumentControlCommand(
            OperationIds::playback::set_last_position, context,
            [this, tick](DocumentSession &session, const bool validateOnly) {
                if (!std::isfinite(tick) || tick < 0.0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("tick"),
                        QStringLiteral("Last playback position is invalid")));
                }
                if (!m_services.snapshot || !m_services.setLastPosition)
                    return AutomationResult<MutationResult>(
                        unavailable(QStringLiteral("Playback host is unavailable")));
                const bool changed = m_services.snapshot().lastPosition != tick;
                if (!validateOnly && changed)
                    m_services.setLastPosition(tick);
                return AutomationResult<MutationResult>(
                    hostMutation(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult>
        PlaybackAutomationFacade::setLoop(const CommandContext &context,
                                          const LoopSettings &settings) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::playback::set_loop, context,
            [this, settings](DocumentSession &session, const bool validateOnly) {
                if (settings.start < 0 || settings.length < 0 ||
                    (settings.enabled && settings.length == 0)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("loop"), QStringLiteral("Loop range is invalid")));
                }
                return commitLoop(session, settings, validateOnly);
            });
    }

    AutomationResult<MutationResult>
        PlaybackAutomationFacade::setLoopEnabled(const CommandContext &context,
                                                 const bool enabled) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::playback::set_loop_enabled, context,
            [this, enabled](DocumentSession &session, const bool validateOnly) {
                if (!m_services.snapshot || !m_services.setLoop)
                    return AutomationResult<MutationResult>(
                        unavailable(QStringLiteral("Playback host is unavailable")));
                auto settings = m_services.snapshot().loop;
                if (enabled && settings.length <= 0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("enabled"),
                        QStringLiteral("Loop range must be initialized before enabling")));
                }
                settings.enabled = enabled;
                return commitLoop(session, settings, validateOnly);
            });
    }

    AutomationResult<MutationResult>
        PlaybackAutomationFacade::clearLoop(const CommandContext &context) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::playback::clear_loop, context,
            [this](DocumentSession &session, const bool validateOnly) {
                if (!m_services.snapshot || !m_services.setLoop)
                    return AutomationResult<MutationResult>(
                        unavailable(QStringLiteral("Playback host is unavailable")));
                const LoopSettings cleared;
                return commitLoop(session, cleared, validateOnly);
            });
    }

    AutomationResult<MutationResult>
        PlaybackAutomationFacade::commitLoop(DocumentSession &session, const LoopSettings &settings,
                                             const bool validateOnly) {
        if (!m_services.snapshot || !m_services.setLoop)
            return unavailable(QStringLiteral("Playback host is unavailable"));
        const auto previous = m_services.snapshot().loop;
        const bool changed = previous != settings;
        if (validateOnly)
            return m_committer.preview(session, changed);
        if (!changed)
            return m_committer.unchanged(session);
        auto actions =
            std::make_unique<SetLoopSettingsActions>(previous, settings, m_services.setLoop);
        return m_committer.commit(session, std::move(actions));
    }

} // namespace Automation
