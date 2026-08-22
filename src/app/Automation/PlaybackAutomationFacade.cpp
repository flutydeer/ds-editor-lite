#include "PlaybackAutomationFacade.h"

#include <cmath>

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
    }

    PlaybackAutomationFacade::PlaybackAutomationFacade(OperationCatalog &catalog,
                                                       AutomationDispatcher &dispatcher,
                                                       PlaybackRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<PlaybackSnapshotDto>
    PlaybackAutomationFacade::getPlayback(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<PlaybackSnapshotDto>(
            QStringLiteral("playback.get"), documentId, [this](DocumentSession &session) {
                if (!m_services.snapshot)
                    return AutomationResult<PlaybackSnapshotDto>(
                        unavailable(QStringLiteral("Playback host is unavailable")));
                const auto host = m_services.snapshot();
                PlaybackSnapshotDto result;
                result.state = host.state;
                result.position = host.position;
                result.lastPosition = host.lastPosition;
                result.document = session.version();
                return AutomationResult<PlaybackSnapshotDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult>
    PlaybackAutomationFacade::play(const CommandContext &context) {
        return setState(QStringLiteral("playback.play"), context, PlaybackState::Playing);
    }

    AutomationResult<MutationResult>
    PlaybackAutomationFacade::pause(const CommandContext &context) {
        return setState(QStringLiteral("playback.pause"), context, PlaybackState::Paused);
    }

    AutomationResult<MutationResult>
    PlaybackAutomationFacade::stop(const CommandContext &context) {
        return setState(QStringLiteral("playback.stop"), context, PlaybackState::Stopped);
    }

    AutomationResult<MutationResult> PlaybackAutomationFacade::setState(
        const OperationId &operationId, const CommandContext &context, const PlaybackState state) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context, QByteArray::number(static_cast<int>(state)),
            [this, state](DocumentSession &session, const bool validateOnly) {
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
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("playback.set_position"), context, QByteArray::number(tick, 'g', 17),
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

    AutomationResult<MutationResult>
    PlaybackAutomationFacade::setLastPosition(const CommandContext &context, const double tick) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("playback.set_last_position"), context,
            QByteArray::number(tick, 'g', 17),
            [this, tick](DocumentSession &session, const bool validateOnly) {
                if (!std::isfinite(tick) || tick < 0.0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("tick"), QStringLiteral("Last playback position is invalid")));
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

    void PlaybackAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = QStringLiteral("playback.get"),
            .category = QStringLiteral("playback"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.DocumentRef.v1"),
            .outputContract = QStringLiteral("automation.PlaybackSnapshot.v1"),
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        const auto addCommand = [&add](const QString &id, const QString &contract) {
            add({
                .id = id,
                .category = QStringLiteral("playback"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .inputContract = contract,
                .outputContract = QStringLiteral("automation.MutationResult.v1"),
                .documentPolicy = DocumentPolicy::Read,
                .revisionPolicy = RevisionPolicy::Check,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
        };
        addCommand(QStringLiteral("playback.pause"),
                   QStringLiteral("automation.PlaybackStateCommand.v1"));
        addCommand(QStringLiteral("playback.play"),
                   QStringLiteral("automation.PlaybackStateCommand.v1"));
        addCommand(QStringLiteral("playback.set_last_position"),
                   QStringLiteral("automation.PlaybackPositionCommand.v1"));
        addCommand(QStringLiteral("playback.set_position"),
                   QStringLiteral("automation.PlaybackPositionCommand.v1"));
        addCommand(QStringLiteral("playback.stop"),
                   QStringLiteral("automation.PlaybackStateCommand.v1"));
    }

} // namespace Automation
