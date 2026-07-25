#ifndef LITE_MUSICBASE_MUSICTIME_H
#define LITE_MUSICBASE_MUSICTIME_H

// Fundamental musical time units. The tick is the project-wide time resolution;
// everything in MusicBase (and the document model above it) measures positions
// and durations in ticks.
namespace MusicTime {
    inline constexpr int ticksPerQuarterNote = 480;
    inline constexpr int ticksPerWholeNote = ticksPerQuarterNote * 4;
}

#endif // LITE_MUSICBASE_MUSICTIME_H
