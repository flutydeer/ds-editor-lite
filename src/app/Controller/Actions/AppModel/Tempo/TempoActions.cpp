//
// Created by fluty on 2024/2/7.
//

#include "TempoActions.h"

#include "EditTempoAction.h"
#include "Controller/Actions/AppModel/Clip/EditClipCommonPropertiesAction.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/Track.h"

void TempoActions::editTempo(const double oldTempo, const double newTempo, AppModel *model) {
    addAction(EditTempoAction::build(oldTempo, newTempo, model));

    // Update audio clips' length
    for (const auto track : model->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() == Clip::Audio) {
                const auto audioClip = dynamic_cast<AudioClip *>(clip);
                const auto audioInfo = audioClip->audioInfo();
                Clip::ClipCommonProperties oldArgs(*clip);
                auto newArgs = oldArgs;
                auto chunksPerTick = static_cast<double>(audioInfo.sampleRate) /
                                     audioInfo.chunkSize * 60 / newTempo / 480;

                const auto oldStartInMs = tickToMs(oldArgs.start, oldTempo);
                newArgs.start = msToTick(oldStartInMs, newTempo);

                const auto oldClipStartInMs = tickToMs(oldArgs.clipStart, oldTempo);
                newArgs.clipStart = msToTick(oldClipStartInMs, newTempo);

                auto targetLength = static_cast<int>(audioInfo.frames /
                                                     (audioInfo.sampleRate * 60 / newTempo / 480));
                newArgs.length = targetLength;

                const auto oldClipLenInMs = oldArgs.clipLen * 60 / oldTempo / 480 * 1000;
                auto targetClipLen = static_cast<int>(oldClipLenInMs * 480 * newTempo / 60000);
                newArgs.clipLen = targetClipLen > targetLength ? targetLength : targetClipLen;

                const auto action =
                    EditClipCommonPropertiesAction::build(oldArgs, newArgs, clip, track);
                addAction(action);
            }
        }
    }
}

double TempoActions::tickToMs(const int tick, const double tempo) {
    return tick * 60 / tempo / 480 * 1000;
}

int TempoActions::msToTick(const double ms, const double tempo) {
    return static_cast<int>(ms * 480 * tempo / 60000);
}