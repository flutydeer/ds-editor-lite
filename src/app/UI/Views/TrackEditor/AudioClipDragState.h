#ifndef AUDIOCLIPDRAGSTATE_H
#define AUDIOCLIPDRAGSTATE_H

#include <lite/ProjectModel/AppModel/Clip.h>

class Timeline;

class AudioClipDragState final {
public:
    static AudioClipDragState begin(double trimStartMs, double playLengthMs,
                                    double materialLengthMs, int visibleStartTick,
                                    double grabTick, const Timeline &timeline);

    [[nodiscard]] int visibleStartForCursor(double cursorTick, const Timeline &timeline) const;
    void moveTo(int visibleStartTick, Clip::ClipCommonProperties &properties,
                const Timeline &timeline) const;
    bool resizeLeftTo(int leftTick, int originalRightTick, int minimumVisibleLength,
                      Clip::ClipCommonProperties &properties, const Timeline &timeline);
    bool resizeRightTo(int rightTick, int visibleStartTick, int minimumVisibleLength,
                       Clip::ClipCommonProperties &properties, const Timeline &timeline);
    void writeTruth(Clip::ClipCommonProperties &properties) const;

private:
    void project(int visibleStartTick, Clip::ClipCommonProperties &properties,
                 const Timeline &timeline) const;

    double m_trimStartMs = 0.0;
    double m_playLengthMs = 0.0;
    double m_materialLengthMs = 0.0;
    double m_grabOffsetMs = 0.0;
    double m_materialStartMs = 0.0;
    double m_visibleEndMs = 0.0;
};

#endif // AUDIOCLIPDRAGSTATE_H
