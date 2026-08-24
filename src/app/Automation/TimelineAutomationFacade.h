#ifndef TIMELINEAUTOMATIONFACADE_H
#define TIMELINEAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"

#include <lite/MusicBase/Tempo.h>
#include <lite/MusicBase/TimeSignature.h>
#include <lite/ProjectModel/AppModel/TrackControl.h>

#include <functional>

namespace Automation {

    struct TimelineSnapshotDto {
        DocumentVersion document;
        QList<Tempo> tempos;
        QList<TimeSignature> timeSignatures;
    };

    class TimelineAutomationFacade final {
    public:
        TimelineAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                                 CommandCommitter &committer);

        AutomationResult<TimelineSnapshotDto> getTimeline(const DocumentId &documentId);
        AutomationResult<MutationResult> setTempo(const CommandContext &context, int tick,
                                                  double tempo);
        AutomationResult<MutationResult> deleteTempo(const CommandContext &context, int tick);
        AutomationResult<MutationResult> setTimeSignature(const CommandContext &context,
                                                          int barIndex, int numerator,
                                                          int denominator);
        AutomationResult<MutationResult> deleteTimeSignature(const CommandContext &context,
                                                             int barIndex);
        AutomationResult<MutationResult> setMasterControl(const CommandContext &context,
                                                          const TrackControl &control);
        AutomationResult<TrackControl> getMaster(const DocumentId &documentId);
        AutomationResult<MutationResult> setMasterGain(const CommandContext &context, double gain);
        AutomationResult<MutationResult> setMasterPan(const CommandContext &context, double pan);
        AutomationResult<MutationResult> setMasterMute(const CommandContext &context, bool mute);
        AutomationResult<MutationResult> setMasterSolo(const CommandContext &context, bool solo);

    private:
        AutomationResult<MutationResult> setMasterControl(const OperationId &operationId,
                                                          const CommandContext &context,
                                                          const TrackControl &control);
        AutomationResult<MutationResult>
            mutateMasterControl(const OperationId &operationId, const CommandContext &context,
                                const QByteArray &fingerprint,
                                const std::function<void(TrackControl &)> &mutate);
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
    };

} // namespace Automation

#endif // TIMELINEAUTOMATIONFACADE_H
