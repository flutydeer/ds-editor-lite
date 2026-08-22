#ifndef PLAYBACKAUTOMATIONFACADE_H
#define PLAYBACKAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <functional>

namespace Automation {

    enum class PlaybackState {
        Stopped,
        Playing,
        Paused,
    };

    struct PlaybackHostSnapshot {
        PlaybackState state = PlaybackState::Stopped;
        double position = 0.0;
        double lastPosition = 0.0;
    };

    struct PlaybackSnapshotDto : PlaybackHostSnapshot {
        DocumentVersion document;
    };

    struct PlaybackRuntimeServices {
        std::function<PlaybackHostSnapshot()> snapshot;
        std::function<bool()> canStart;
        std::function<bool()> play;
        std::function<void()> pause;
        std::function<void()> stop;
        std::function<void(double)> setPosition;
        std::function<void(double)> setLastPosition;
    };

    class PlaybackAutomationFacade final {
    public:
        PlaybackAutomationFacade(OperationCatalog &catalog,
                                 AutomationDispatcher &dispatcher,
                                 PlaybackRuntimeServices services = {});

        AutomationResult<PlaybackSnapshotDto> getPlayback(const DocumentId &documentId);
        AutomationResult<MutationResult> play(const CommandContext &context);
        AutomationResult<MutationResult> pause(const CommandContext &context);
        AutomationResult<MutationResult> stop(const CommandContext &context);
        AutomationResult<MutationResult> setPosition(const CommandContext &context,
                                                     double tick);
        AutomationResult<MutationResult> setLastPosition(const CommandContext &context,
                                                         double tick);

    private:
        AutomationResult<MutationResult> setState(const OperationId &operationId,
                                                  const CommandContext &context,
                                                  PlaybackState state);
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        PlaybackRuntimeServices m_services;
    };

} // namespace Automation

#endif // PLAYBACKAUTOMATIONFACADE_H
