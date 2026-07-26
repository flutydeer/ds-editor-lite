#ifndef LITE_MUSICBASE_MUSICTIME_H
#define LITE_MUSICBASE_MUSICTIME_H

#include <QString>

// Fundamental musical time units plus the measure/beat/tick value type. The
// tick is the project-wide time resolution; everything in MusicBase (and the
// document model above it) measures positions and durations in ticks.
//
// A MusicTime is a plain triple; mapping it to/from absolute ticks requires a
// Timeline, because measure lengths depend on the time signature in effect.
// measure and beat are 0-based internally; toString/fromString use the 1-based
// "measure:beat:tick" display convention (tick 0 is "001:01:000").
class MusicTime {
public:
    static constexpr int ticksPerQuarterNote = 480;
    static constexpr int ticksPerWholeNote = ticksPerQuarterNote * 4;

    constexpr MusicTime() = default;

    constexpr MusicTime(const int measure, const int beat, const int tick)
        : measure(measure), beat(beat), tick(tick) {
    }

    int measure = 0;
    int beat = 0;
    int tick = 0;

    [[nodiscard]] constexpr bool isValid() const {
        return measure >= 0 && beat >= 0 && tick >= 0;
    }

    // Parses "015:2:000" style input (1-based measure and beat; ASCII or
    // full-width colons). Omitted trailing components default to the first
    // beat / tick 0. Returns an invalid MusicTime and sets *ok to false on
    // malformed input.
    static MusicTime fromString(QStringView str, bool *ok = nullptr);

    // Formats as "measure:beat:tick" with 1-based measure and beat, zero
    // padded to 3/2/3 digits ("001:01:000"), matching the transport display.
    [[nodiscard]] QString toString() const;

    friend constexpr bool operator==(const MusicTime &lhs, const MusicTime &rhs) {
        return lhs.measure == rhs.measure && lhs.beat == rhs.beat && lhs.tick == rhs.tick;
    }

    friend constexpr bool operator!=(const MusicTime &lhs, const MusicTime &rhs) {
        return !(lhs == rhs);
    }
};

#endif // LITE_MUSICBASE_MUSICTIME_H
