#ifndef AUDIOVIEWCONTENT_H
#define AUDIOVIEWCONTENT_H

#include "AudioLaneGeometry.h"
#include "Global/AppGlobal.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioInfoModel.h>

#include <QString>

// Everything both audio view backends need to draw, in one plain value. The
// container resolves it from the model, the backends only read it, which keeps
// the QGraphicsView and the RHI widget from drifting apart while staying free of
// model wiring inside the renderers.
struct AudioViewContent {
    int clipId = -1;
    // Timeline position of the first sample of the material
    int start = 0;
    // Material hidden before the clip's left edge
    int clipStart = 0;
    // Length of the audible part
    int clipLen = 0;
    // Total material length in ticks
    int length = 0;
    int trackColorIndex = 0;
    QString path;
    QString errorMessage;
    AppGlobal::AudioClipLoadState loadState = AppGlobal::AudioClipLoadState::Loading;
    AudioInfoModel audioInfo;
    Timeline timeline;

    [[nodiscard]] bool isValid() const {
        return clipId >= 0;
    }
    [[nodiscard]] int channels() const {
        return audioInfo.channels;
    }
    [[nodiscard]] int laneCount() const {
        return AudioLaneGeometry::laneCount(channels());
    }
    // First audible tick of the material
    [[nodiscard]] int contentStartTick() const {
        return start + clipStart;
    }
    [[nodiscard]] int contentEndTick() const {
        return start + clipStart + clipLen;
    }
    // End of the whole material, trimmed part included
    [[nodiscard]] int materialEndTick() const {
        return start + length;
    }
};

#endif // AUDIOVIEWCONTENT_H
