#ifndef TRACKSEDITORGLOBAL_H
#define TRACKSEDITORGLOBAL_H

namespace TracksEditorGlobal {
    constexpr int pixelsPerQuarterNote = 32;
    constexpr double trackHeight = 72;
    constexpr int trackListWidth = 320;
    constexpr double narrowModeScaleY = 1;
    constexpr int trackViewHeaderHeight = 36;
    constexpr int infoLaneHeight = 28;         // Height of info lanes below the ruler
    constexpr int loopRegionHeight = 8;        // Height of loop region area
    // Left margin (screen px) before tick 0, kept inside the scene so the
    // playhead triangle/bar do not clip at the left viewport edge. Scene
    // coordinates are already zoom-scaled (px = tick * scaleX * ppq / tpq),
    // so the margin is added as plain pixels - never divided by scaleX - to
    // keep its on-screen width constant at any zoom. Half of the ruler
    // triangle (TimelineView w=12) is 6px.
    constexpr int trackViewLeftMargin = 10;
};

#endif //TRACKSEDITORGLOBAL_H
