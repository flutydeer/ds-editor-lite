#ifndef DS_EDITOR_LITE_TIMELINE_H
#define DS_EDITOR_LITE_TIMELINE_H

#include "MusicTime.h"
#include "Tempo.h"
#include "TimeSignature.h"

#include <QList>

struct TempoChangeRange {
    int startTick = 0;
    int endTick = 0; // exclusive; INT_MAX denotes the unbounded tail

    [[nodiscard]] bool intersects(int start, int end) const {
        return start < endTick && end > startTick;
    }
};

// The project time axis: a tempo map indexed by tick and a time signature map
// indexed by measure number. Both lists are kept sorted, deduplicated and
// always contain a point at position 0 — that anchor point can be edited but
// never removed; every conversion below relies on this invariant.
//
// Tempi control the tick<->wall-clock mapping (piecewise linear per segment);
// time signatures control the tick<->measure/beat mapping. The two sides are
// completely independent of each other.
class Timeline {
public:
    Timeline();
    explicit Timeline(QList<Tempo> tempos, QList<TimeSignature> timeSignatures = {});

    // --- tempo map (indexed by tick) ---
    [[nodiscard]] const QList<Tempo> &tempos() const;
    void setTempos(QList<Tempo> tempos);
    // Inserts a point, or replaces the value of an existing point at the same tick.
    void addTempo(const Tempo &tempo);
    // Removes the point at exactly `tick`; refuses tick 0 and unknown positions.
    bool removeTempoAt(int tick);
    [[nodiscard]] double tempoAt(double tick) const;
    // The tick of the tempo point governing `tick` (the nearest point at or before it).
    [[nodiscard]] int nearestTickWithTempoTo(int tick) const;
    // Effective tick ranges whose governing BPM differs between two maps.
    // Redundant same-value points therefore produce no invalidation.
    [[nodiscard]] static QList<TempoChangeRange> tempoChangeRanges(const Timeline &before,
                                                                   const Timeline &after);

    // --- time signature map (indexed by measure number) ---
    [[nodiscard]] const QList<TimeSignature> &timeSignatures() const;
    void setTimeSignatures(QList<TimeSignature> timeSignatures);
    // Inserts a point, or replaces the meter of an existing point at the same bar.
    void addTimeSignature(const TimeSignature &timeSignature);
    // Removes the point at exactly `barIndex`; refuses bar 0 and unknown positions.
    bool removeTimeSignatureAt(int barIndex);
    [[nodiscard]] TimeSignature timeSignatureAt(int bar) const;
    // The bar of the signature point governing `bar` (the nearest point at or before it).
    [[nodiscard]] int nearestBarWithTimeSignatureTo(int bar) const;

    // --- tick <-> wall clock (double: sub-tick precision is load-bearing for inference) ---
    [[nodiscard]] double tickToMs(double tick) const;
    [[nodiscard]] double msToTick(double ms) const;
    [[nodiscard]] double tickToSec(double tick) const;
    [[nodiscard]] double secToTick(double sec) const;

    // --- tick <-> measure/beat ---
    [[nodiscard]] int barToTick(int bar) const;         // -1 if bar < 0
    [[nodiscard]] MusicTime tickToTime(int tick) const; // invalid if tick < 0
    // Beats may exceed the measure's numerator and simply spill forward using
    // that measure's beat length; any negative component yields -1.
    [[nodiscard]] int timeToTick(const MusicTime &time) const;
    [[nodiscard]] int timeToTick(int measure, int beat, int tick) const;
    [[nodiscard]] QString getBarBeatTickTime(int ticks) const;

    friend bool operator==(const Timeline &lhs, const Timeline &rhs);

    friend bool operator!=(const Timeline &lhs, const Timeline &rhs);

private:
    void normalizeTempos();
    void normalizeTimeSignatures();
    void rebuildTempoCache();
    void rebuildTimeSignatureCache();
    [[nodiscard]] qsizetype tempoIndexAtTick(double tick) const;
    [[nodiscard]] qsizetype tempoIndexAtMs(double ms) const;
    [[nodiscard]] qsizetype signatureIndexAtBar(int bar) const;
    [[nodiscard]] qsizetype signatureIndexAtTick(int tick) const;

    QList<Tempo> m_tempos;
    QList<TimeSignature> m_timeSignatures;
    // Parallel prefix tables (one entry per point above), searched with
    // std::upper_bound. Kept as sorted lists rather than floating-point-keyed
    // maps to avoid precision pitfalls in reverse lookups.
    QList<double> m_msAtTempo;    // accumulated ms at m_tempos[i].pos
    // Index of the first point in m_tempos[i]'s maximal same-value run. tickToMs
    // anchors to it so that redundant same-value points (legal, editable) do not
    // perturb the conversion via floating-point non-associativity.
    QList<qsizetype> m_tempoRunStart;
    QList<int> m_tickAtSignature; // tick where bar m_timeSignatures[i].barIndex starts
};


#endif // DS_EDITOR_LITE_TIMELINE_H
