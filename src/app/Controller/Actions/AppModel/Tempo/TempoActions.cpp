//
// Created by fluty on 2024/2/7.
//

#include "TempoActions.h"

#include "EditTempoAction.h"
#include "Controller/Actions/AppModel/Clip/EditClipCommonPropertiesAction.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"

void TempoActions::editTempo(double oldTempo, double newTempo, AppModel *model) {
    addAction(EditTempoAction::build(oldTempo, newTempo, model));

    // Update audio clips' length
    for (const auto track : model->tracks()) {
        for (auto clip : track->clips()) {
            if (clip->clipType() == Clip::Audio) {
                auto audioClip = dynamic_cast<AudioClip *>(clip);
                auto audioInfo = audioClip->audioInfo();
                Clip::ClipCommonProperties oldArgs(*clip);
                auto newArgs = oldArgs;
                auto chunksPerTick = static_cast<double>(audioInfo.sampleRate) /
                                     audioInfo.chunkSize * 60 / newTempo / 480;

                auto oldStartInMs = tickToMs(oldArgs.start, oldTempo);
                newArgs.start = msToTick(oldStartInMs, newTempo);

                auto oldClipStartInMs = tickToMs(oldArgs.clipStart, oldTempo);
                newArgs.clipStart = msToTick(oldClipStartInMs, newTempo);

                auto targetLength = static_cast<int>(audioInfo.frames /
                                                     (audioInfo.sampleRate * 60 / newTempo / 480));
                newArgs.length = targetLength;

                auto oldClipLenInMs = oldArgs.clipLen * 60 / oldTempo / 480 * 1000;
                auto targetClipLen = static_cast<int>(oldClipLenInMs * 480 * newTempo / 60000);
                newArgs.clipLen = targetClipLen > targetLength ? targetLength : targetClipLen;

                auto action = EditClipCommonPropertiesAction::build(oldArgs, newArgs, clip, track);
                addAction(action);
            }
        }
    }
}
double TempoActions::tickToMs(int tick, double tempo) {
    return tick * 60 / tempo / 480 * 1000;
}
int TempoActions::msToTick(double ms, double tempo) {
    return static_cast<int>(ms * 480 * tempo / 60000);
}