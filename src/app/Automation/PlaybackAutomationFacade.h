#ifndef PLAYBACKAUTOMATIONFACADE_H
#define PLAYBACKAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"

#include <lite/ProjectModel/AppModel/LoopSettings.h>

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
        LoopSettings loop;
    };

    struct PlaybackSnapshotDto : PlaybackHostSnapshot {
        DocumentVersion document;
        bool playable = false;
    };

    struct PlaybackRuntimeServices {
        std::function<PlaybackHostSnapshot()> snapshot;
        std::function<bool()> canStart;
        std::function<bool()> play;
        std::function<void()> pause;
        std::function<void()> stop;
        std::function<void(double)> setPosition;
        std::function<void(double)> setLastPosition;
        std::function<void(const LoopSettings &)> setLoop;
    };

    class PlaybackAutomationFacade final {
    public:
        PlaybackAutomationFacade(AutomationDispatcher &dispatcher, CommandCommitter &committer,
                                 PlaybackRuntimeServices services = {});

        AutomationResult<PlaybackSnapshotDto> getPlayback(const DocumentId &documentId);
        AutomationResult<MutationResult> play(const CommandContext &context);
        AutomationResult<MutationResult> pause(const CommandContext &context);
        AutomationResult<MutationResult> stop(const CommandContext &context);
        AutomationResult<MutationResult> setPosition(const CommandContext &context, double tick);
        AutomationResult<MutationResult> seek(const CommandContext &context, double tick);
        AutomationResult<MutationResult> setLastPosition(const CommandContext &context,
                                                         double tick);
        AutomationResult<MutationResult> setLoop(const CommandContext &context,
                                                 const LoopSettings &settings);
        AutomationResult<MutationResult> setLoopEnabled(const CommandContext &context,
                                                        bool enabled);
        AutomationResult<MutationResult> clearLoop(const CommandContext &context);

    private:
        AutomationResult<MutationResult> setState(const OperationId &operationId,
                                                  const CommandContext &context,
                                                  PlaybackState state);
        AutomationResult<MutationResult>
            commitLoop(DocumentSession &session, const LoopSettings &settings, bool validateOnly);

        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        PlaybackRuntimeServices m_services;
    };

} // namespace Automation

#endif // PLAYBACKAUTOMATIONFACADE_H
