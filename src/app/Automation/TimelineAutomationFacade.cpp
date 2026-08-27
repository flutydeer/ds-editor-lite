#include "TimelineAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/MasterControl/MasterControlActions.h"
#include "Controller/Actions/AppModel/Tempo/TempoActions.h"
#include "Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QDataStream>
#include <QIODevice>

#include <algorithm>
#include <cmath>
#include <memory>

namespace Automation {
    namespace {
        bool isPowerOfTwo(const int value) {
            return value > 0 && (value & (value - 1)) == 0;
        }

        bool controlsEqual(const TrackControl &left, const TrackControl &right) {
            return left.gain() == right.gain() && left.pan() == right.pan() &&
                   left.mute() == right.mute() && left.solo() == right.solo();
        }

        QByteArray tempoFingerprint(const int tick, const double tempo) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << tick << tempo;
            return result;
        }

        QByteArray timeSignatureFingerprint(const int barIndex, const int numerator,
                                            const int denominator) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << barIndex << numerator << denominator;
            return result;
        }

        QByteArray masterControlFingerprint(const TrackControl &control) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << control.gain() << control.pan() << control.mute() << control.solo();
            return result;
        }
    }

    TimelineAutomationFacade::TimelineAutomationFacade(OperationCatalog &catalog,
                                                       AutomationDispatcher &dispatcher,
                                                       CommandCommitter &committer)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_committer(committer) {
        registerOperations();
    }

    AutomationResult<TimelineSnapshotDto>
        TimelineAutomationFacade::getTimeline(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<TimelineSnapshotDto>(
            OperationIds::timeline::get, documentId, [](DocumentSession &session) {
                const auto &timeline = session.model()->timeline();
                TimelineSnapshotDto result;
                result.document = session.version();
                result.tempos = timeline.tempos();
                result.timeSignatures = timeline.timeSignatures();
                return AutomationResult<TimelineSnapshotDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setTempo(const CommandContext &context, const int tick,
                                           const double tempo) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tempos::set, context, tempoFingerprint(tick, tempo),
            [this, tick, tempo](DocumentSession &session, const bool validateOnly) {
                if (tick < 0 || !std::isfinite(tempo) || tempo <= 0.0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("tempo"), QStringLiteral("Tempo and tick are invalid")));
                }

                auto *model = session.model();
                const auto &tempos = model->timeline().tempos();
                const auto existing =
                    std::find_if(tempos.cbegin(), tempos.cend(),
                                 [tick](const Tempo &candidate) { return candidate.pos == tick; });
                const bool changed = existing == tempos.cend() || existing->value != tempo;
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, changed));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto actions = std::make_unique<TempoActions>();
                actions->setTempoAt({tick, tempo}, model);
                return m_committer.commit(session, std::move(actions));
            });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::deleteTempo(const CommandContext &context, const int tick) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tempos::remove, context, tempoFingerprint(tick, 0.0),
            [this, tick](DocumentSession &session, const bool validateOnly) {
                if (tick <= 0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("tick"),
                        QStringLiteral("The tempo anchor cannot be deleted")));
                }

                auto *model = session.model();
                const auto &tempos = model->timeline().tempos();
                const bool changed =
                    std::any_of(tempos.cbegin(), tempos.cend(),
                                [tick](const Tempo &candidate) { return candidate.pos == tick; });
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, changed));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto actions = std::make_unique<TempoActions>();
                actions->removeTempoAt(tick, model);
                return m_committer.commit(session, std::move(actions));
            });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setTimeSignature(const CommandContext &context,
                                                   const int barIndex, const int numerator,
                                                   const int denominator) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::time_signatures::set, context,
            timeSignatureFingerprint(barIndex, numerator, denominator),
            [this, barIndex, numerator, denominator](DocumentSession &session,
                                                     const bool validateOnly) {
                if (barIndex < 0 || numerator <= 0 || !isPowerOfTwo(denominator)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("time_signature"),
                        QStringLiteral("Time signature values are invalid")));
                }

                auto *model = session.model();
                const auto &signatures = model->timeline().timeSignatures();
                const auto existing = std::find_if(
                    signatures.cbegin(), signatures.cend(),
                    [barIndex](const TimeSignature &value) { return value.barIndex == barIndex; });
                const bool changed = existing == signatures.cend() ||
                                     existing->numerator != numerator ||
                                     existing->denominator != denominator;
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, changed));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto actions = std::make_unique<TimeSignatureActions>();
                actions->setTimeSignatureAt(TimeSignature(barIndex, numerator, denominator), model);
                return m_committer.commit(session, std::move(actions));
            });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::deleteTimeSignature(const CommandContext &context,
                                                      const int barIndex) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::time_signatures::remove, context,
            timeSignatureFingerprint(barIndex, 0, 0),
            [this, barIndex](DocumentSession &session, const bool validateOnly) {
                if (barIndex <= 0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("bar_index"),
                        QStringLiteral("The time-signature anchor cannot be deleted")));
                }

                auto *model = session.model();
                const auto &signatures = model->timeline().timeSignatures();
                const bool changed = std::any_of(
                    signatures.cbegin(), signatures.cend(),
                    [barIndex](const TimeSignature &value) { return value.barIndex == barIndex; });
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, changed));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto actions = std::make_unique<TimeSignatureActions>();
                actions->removeTimeSignatureAt(barIndex, model);
                return m_committer.commit(session, std::move(actions));
            });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterControl(const CommandContext &context,
                                                   const TrackControl &control) {
        return setMasterControl(OperationIds::master::set_control, context, control);
    }

    AutomationResult<TrackControl>
        TimelineAutomationFacade::getMaster(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<TrackControl>(
            OperationIds::master::get, documentId, [](DocumentSession &session) {
                return AutomationResult<TrackControl>(session.model()->masterControl());
            });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterGain(const CommandContext &context, const double gain) {
        return mutateMasterControl(OperationIds::master::set_gain, context,
                                   QByteArray::number(gain, 'g', 17),
                                   [gain](TrackControl &control) { control.setGain(gain); });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterPan(const CommandContext &context, const double pan) {
        return mutateMasterControl(OperationIds::master::set_pan, context,
                                   QByteArray::number(pan, 'g', 17),
                                   [pan](TrackControl &control) { control.setPan(pan); });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterMute(const CommandContext &context, const bool mute) {
        return mutateMasterControl(OperationIds::master::set_mute, context,
                                   QByteArray::number(mute),
                                   [mute](TrackControl &control) { control.setMute(mute); });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterSolo(const CommandContext &context, const bool solo) {
        return mutateMasterControl(OperationIds::master::set_solo, context,
                                   QByteArray::number(solo),
                                   [solo](TrackControl &control) { control.setSolo(solo); });
    }

    AutomationResult<MutationResult> TimelineAutomationFacade::mutateMasterControl(
        const OperationId &operationId, const CommandContext &context,
        const QByteArray &requestFingerprint, const std::function<void(TrackControl &)> &mutate) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context, requestFingerprint,
            [this, mutate](DocumentSession &session, const bool validateOnly) {
                auto control = session.model()->masterControl();
                mutate(control);
                if (!std::isfinite(control.gain()) || !std::isfinite(control.pan()) ||
                    control.pan() < -1.0 || control.pan() > 1.0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("value"), QStringLiteral("Master control is invalid")));
                }
                const bool changed = !controlsEqual(session.model()->masterControl(), control);
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, changed));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<MasterControlActions>();
                actions->editMasterControl(control, session.model());
                return m_committer.commit(session, std::move(actions));
            });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterControl(const OperationId &operationId,
                                                   const CommandContext &context,
                                                   const TrackControl &control) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context, masterControlFingerprint(control),
            [this, control](DocumentSession &session, const bool validateOnly) {
                if (!std::isfinite(control.gain()) || !std::isfinite(control.pan())) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("control"), QStringLiteral("Master control is invalid")));
                }

                auto *model = session.model();
                const bool changed = !controlsEqual(model->masterControl(), control);
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, changed));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto actions = std::make_unique<MasterControlActions>();
                actions->editMasterControl(control, model);
                return m_committer.commit(session, std::move(actions));
            });
    }

    void TimelineAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };

        add({
            .id = OperationIds::timeline::get,
            .category = QStringLiteral("timeline"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        add({
            .id = OperationIds::master::get,
            .category = QStringLiteral("master"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });

        const auto addMutation = [&add](const OperationId &id) {
            add({
                .id = id,
                .category = id.section('.', 0, 0),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::Write,
                .revisionPolicy = RevisionPolicy::Increment,
                .historyPolicy = HistoryPolicy::Record,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::DocumentGeneration,
            });
        };
        addMutation(OperationIds::master::set_control);
        addMutation(OperationIds::master::set_gain);
        addMutation(OperationIds::master::set_mute);
        addMutation(OperationIds::master::set_pan);
        addMutation(OperationIds::master::set_solo);
        addMutation(OperationIds::tempos::remove);
        addMutation(OperationIds::tempos::set);
        addMutation(OperationIds::time_signatures::remove);
        addMutation(OperationIds::time_signatures::set);
    }

} // namespace Automation
