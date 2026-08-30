#include "TimelineAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/MasterControl/MasterControlActions.h"
#include "Controller/Actions/AppModel/Tempo/TempoActions.h"
#include "Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace Automation {
    namespace {
        bool isPowerOfTwo(const int value) {
            return value > 0 && (value & (value - 1)) == 0;
        }

        bool validTimeSignatureProjection(QList<TimeSignature> signatures) {
            std::sort(signatures.begin(), signatures.end(), [](const TimeSignature &left,
                                                               const TimeSignature &right) {
                return left.barIndex < right.barIndex;
            });

            qint64 tick = 0;
            for (qsizetype index = 0; index < signatures.size(); ++index) {
                const auto &signature = signatures.at(index);
                const auto ticksPerBar =
                    static_cast<qint64>(signature.ticksPerBeat()) * signature.numerator;
                if (ticksPerBar <= 0 || ticksPerBar > std::numeric_limits<int>::max())
                    return false;
                if (index == 0)
                    continue;
                const auto &previous = signatures.at(index - 1);
                const auto previousTicksPerBar =
                    static_cast<qint64>(previous.ticksPerBeat()) * previous.numerator;
                tick += (static_cast<qint64>(signature.barIndex) - previous.barIndex) *
                        previousTicksPerBar;
                if (tick > std::numeric_limits<int>::max())
                    return false;
            }
            return true;
        }

        bool validTimeSignatureProjection(QList<TimeSignature> signatures,
                                          const TimeSignature &candidate) {
            const auto existing = std::find_if(
                signatures.begin(), signatures.end(), [&candidate](const TimeSignature &value) {
                    return value.barIndex == candidate.barIndex;
                });
            if (existing == signatures.end())
                signatures.append(candidate);
            else
                *existing = candidate;
            return validTimeSignatureProjection(std::move(signatures));
        }

        bool controlsEqual(const TrackControl &left, const TrackControl &right) {
            return left.gain() == right.gain() && left.pan() == right.pan() &&
                   left.mute() == right.mute() && left.solo() == right.solo();
        }
    }

    TimelineAutomationFacade::TimelineAutomationFacade(AutomationDispatcher &dispatcher,
                                                       CommandCommitter &committer)
        : m_dispatcher(dispatcher), m_committer(committer) {
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
            OperationIds::tempos::set, context,
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
            OperationIds::tempos::remove, context,
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
            [this, barIndex, numerator, denominator](DocumentSession &session,
                                                     const bool validateOnly) {
                if (barIndex < 0 || numerator <= 0 || !isPowerOfTwo(denominator)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("time_signature"),
                        QStringLiteral("Time signature values are invalid")));
                }

                auto *model = session.model();
                const auto &signatures = model->timeline().timeSignatures();
                const TimeSignature candidate(barIndex, numerator, denominator);
                if (!validTimeSignatureProjection(signatures, candidate)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("time_signature"),
                        QStringLiteral("Time signature position is out of bounds")));
                }
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
                actions->setTimeSignatureAt(candidate, model);
                return m_committer.commit(session, std::move(actions));
            });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::deleteTimeSignature(const CommandContext &context,
                                                      const int barIndex) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::time_signatures::remove, context,
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
                if (!changed)
                    return AutomationResult<MutationResult>(
                        validateOnly ? m_committer.preview(session, false)
                                     : m_committer.unchanged(session));

                auto projected = signatures;
                projected.removeIf([barIndex](const TimeSignature &value) {
                    return value.barIndex == barIndex;
                });
                if (!validTimeSignatureProjection(std::move(projected))) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("bar_index"),
                        QStringLiteral("Removing the time signature would move a later signature "
                                       "out of bounds")));
                }
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, true));

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
                                   [gain](TrackControl &control) { control.setGain(gain); });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterPan(const CommandContext &context, const double pan) {
        return mutateMasterControl(OperationIds::master::set_pan, context,
                                   [pan](TrackControl &control) { control.setPan(pan); });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterMute(const CommandContext &context, const bool mute) {
        return mutateMasterControl(OperationIds::master::set_mute, context,
                                   [mute](TrackControl &control) { control.setMute(mute); });
    }

    AutomationResult<MutationResult>
        TimelineAutomationFacade::setMasterSolo(const CommandContext &context, const bool solo) {
        return mutateMasterControl(OperationIds::master::set_solo, context,
                                   [solo](TrackControl &control) { control.setSolo(solo); });
    }

    AutomationResult<MutationResult> TimelineAutomationFacade::mutateMasterControl(
        const OperationId &operationId, const CommandContext &context,
        const std::function<void(TrackControl &)> &mutate) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context,
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
            operationId, context,
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

} // namespace Automation
