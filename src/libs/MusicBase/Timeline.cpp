#include "Timeline.h"

#include <QDebug>
#include <QSet>

#include <algorithm>
#include <limits>

// Conversion formulas intentionally keep the exact operation order of the old
// single-tempo MusicTimeConverter so that a single-point timeline stays
// bit-identical to the previous behavior (degenerate equivalence).

Timeline::Timeline() : Timeline({}, {}) {
}

Timeline::Timeline(QList<Tempo> tempos, QList<TimeSignature> timeSignatures)
    : m_tempos(std::move(tempos)), m_timeSignatures(std::move(timeSignatures)) {
    normalizeTempos();
    normalizeTimeSignatures();
    rebuildTempoCache();
    rebuildTimeSignatureCache();
}

const QList<Tempo> &Timeline::tempos() const {
    return m_tempos;
}

void Timeline::setTempos(QList<Tempo> tempos) {
    m_tempos = std::move(tempos);
    normalizeTempos();
    rebuildTempoCache();
}

void Timeline::addTempo(const Tempo &tempo) {
    if (tempo.pos < 0 || tempo.value <= 0) {
        qWarning() << "Timeline: ignoring invalid tempo point at" << tempo.pos << "value"
                   << tempo.value;
        return;
    }
    const auto it =
        std::lower_bound(m_tempos.begin(), m_tempos.end(), tempo.pos,
                         [](const Tempo &item, const int pos) { return item.pos < pos; });
    if (it != m_tempos.end() && it->pos == tempo.pos)
        it->value = tempo.value;
    else
        m_tempos.insert(it, tempo);
    rebuildTempoCache();
}

bool Timeline::removeTempoAt(const int tick) {
    if (tick == 0)
        return false; // The anchor point at tick 0 is not removable.
    const auto it =
        std::lower_bound(m_tempos.begin(), m_tempos.end(), tick,
                         [](const Tempo &item, const int pos) { return item.pos < pos; });
    if (it == m_tempos.end() || it->pos != tick)
        return false;
    m_tempos.erase(it);
    rebuildTempoCache();
    return true;
}

double Timeline::tempoAt(const double tick) const {
    return m_tempos[tempoIndexAtTick(tick)].value;
}

int Timeline::nearestTickWithTempoTo(const int tick) const {
    return m_tempos[tempoIndexAtTick(tick)].pos;
}

QList<TempoChangeRange> Timeline::tempoChangeRanges(const Timeline &before, const Timeline &after) {
    QSet<int> breakpointSet;
    for (const auto &tempo : before.tempos())
        breakpointSet.insert(tempo.pos);
    for (const auto &tempo : after.tempos())
        breakpointSet.insert(tempo.pos);

    QList<int> breakpoints(breakpointSet.cbegin(), breakpointSet.cend());
    std::sort(breakpoints.begin(), breakpoints.end());

    QList<TempoChangeRange> ranges;
    for (qsizetype i = 0; i < breakpoints.size(); ++i) {
        const int start = breakpoints.at(i);
        const int end =
            i + 1 < breakpoints.size() ? breakpoints.at(i + 1) : std::numeric_limits<int>::max();
        if (before.tempoAt(start) == after.tempoAt(start))
            continue;
        if (!ranges.isEmpty() && ranges.last().endTick == start)
            ranges.last().endTick = end;
        else
            ranges.append({start, end});
    }
    return ranges;
}

const QList<TimeSignature> &Timeline::timeSignatures() const {
    return m_timeSignatures;
}

void Timeline::setTimeSignatures(QList<TimeSignature> timeSignatures) {
    m_timeSignatures = std::move(timeSignatures);
    normalizeTimeSignatures();
    rebuildTimeSignatureCache();
}

void Timeline::addTimeSignature(const TimeSignature &timeSignature) {
    if (timeSignature.barIndex < 0 || !timeSignature.isValid()) {
        qWarning() << "Timeline: ignoring invalid time signature at bar" << timeSignature.barIndex
                   << timeSignature.numerator << "/" << timeSignature.denominator;
        return;
    }
    const auto it = std::lower_bound(
        m_timeSignatures.begin(), m_timeSignatures.end(), timeSignature.barIndex,
        [](const TimeSignature &item, const int bar) { return item.barIndex < bar; });
    if (it != m_timeSignatures.end() && it->barIndex == timeSignature.barIndex) {
        it->numerator = timeSignature.numerator;
        it->denominator = timeSignature.denominator;
    } else {
        m_timeSignatures.insert(it, timeSignature);
    }
    rebuildTimeSignatureCache();
}

bool Timeline::removeTimeSignatureAt(const int barIndex) {
    if (barIndex == 0)
        return false; // The anchor point at bar 0 is not removable.
    const auto it = std::lower_bound(
        m_timeSignatures.begin(), m_timeSignatures.end(), barIndex,
        [](const TimeSignature &item, const int bar) { return item.barIndex < bar; });
    if (it == m_timeSignatures.end() || it->barIndex != barIndex)
        return false;
    m_timeSignatures.erase(it);
    rebuildTimeSignatureCache();
    return true;
}

TimeSignature Timeline::timeSignatureAt(const int bar) const {
    return m_timeSignatures[signatureIndexAtBar(bar)];
}

int Timeline::nearestBarWithTimeSignatureTo(const int bar) const {
    return m_timeSignatures[signatureIndexAtBar(bar)].barIndex;
}

double Timeline::tickToMs(const double tick) const {
    // Anchor to the start of the same-value run so redundant tempo points do not
    // change the result (see rebuildTempoCache / TestParamResample).
    const auto r = m_tempoRunStart[tempoIndexAtTick(tick)];
    const auto &tempo = m_tempos[r];
    return m_msAtTempo[r] +
           (tick - tempo.pos) * 60 / tempo.value / MusicTime::ticksPerQuarterNote * 1000;
}

double Timeline::msToTick(const double ms) const {
    const auto r = m_tempoRunStart[tempoIndexAtMs(ms)];
    const auto &tempo = m_tempos[r];
    return tempo.pos + (ms - m_msAtTempo[r]) * MusicTime::ticksPerQuarterNote * tempo.value / 60000;
}

double Timeline::tickToSec(const double tick) const {
    return tickToMs(tick) / 1000.0;
}

double Timeline::secToTick(const double sec) const {
    return msToTick(sec * 1000.0);
}

int Timeline::barToTick(const int bar) const {
    if (bar < 0)
        return -1;
    const auto i = signatureIndexAtBar(bar);
    const auto &signature = m_timeSignatures[i];
    return m_tickAtSignature[i] + (bar - signature.barIndex) * signature.ticksPerBar();
}

MusicTime Timeline::tickToTime(const int tick) const {
    if (tick < 0)
        return {-1, -1, -1};
    const auto i = signatureIndexAtTick(tick);
    const auto &signature = m_timeSignatures[i];
    const int delta = tick - m_tickAtSignature[i];
    const int barTicks = signature.ticksPerBar();
    const int beatTicks = signature.ticksPerBeat();
    return {signature.barIndex + delta / barTicks, delta % barTicks / beatTicks,
            delta % barTicks % beatTicks};
}

int Timeline::timeToTick(const MusicTime &time) const {
    return timeToTick(time.measure, time.beat, time.tick);
}

int Timeline::timeToTick(const int measure, const int beat, const int tick) const {
    if (measure < 0 || beat < 0 || tick < 0)
        return -1;
    const auto &signature = m_timeSignatures[signatureIndexAtBar(measure)];
    return barToTick(measure) + beat * signature.ticksPerBeat() + tick;
}

QString Timeline::getBarBeatTickTime(const int ticks) const {
    return tickToTime(qMax(0, ticks)).toString();
}

bool operator==(const Timeline &lhs, const Timeline &rhs) {
    return lhs.m_tempos == rhs.m_tempos && lhs.m_timeSignatures == rhs.m_timeSignatures;
}

bool operator!=(const Timeline &lhs, const Timeline &rhs) {
    return !(lhs == rhs);
}

void Timeline::normalizeTempos() {
    std::stable_sort(m_tempos.begin(), m_tempos.end(),
                     [](const Tempo &lhs, const Tempo &rhs) { return lhs.pos < rhs.pos; });
    QList<Tempo> cleaned;
    cleaned.reserve(m_tempos.size());
    for (const auto &tempo : m_tempos) {
        if (tempo.pos < 0 || tempo.value <= 0) {
            qWarning() << "Timeline: dropping invalid tempo point at" << tempo.pos << "value"
                       << tempo.value;
            continue;
        }
        if (!cleaned.isEmpty() && cleaned.last().pos == tempo.pos) {
            qWarning() << "Timeline: dropping duplicate tempo point at" << tempo.pos;
            continue;
        }
        cleaned.append(tempo);
    }
    if (cleaned.isEmpty()) {
        cleaned.append(Tempo{});
    } else if (cleaned.first().pos != 0) {
        qWarning() << "Timeline: no tempo point at tick 0; anchoring the earliest point";
        auto anchor = cleaned.first();
        anchor.pos = 0;
        cleaned.prepend(anchor);
    }
    m_tempos = std::move(cleaned);
}

void Timeline::normalizeTimeSignatures() {
    std::stable_sort(m_timeSignatures.begin(), m_timeSignatures.end(),
                     [](const TimeSignature &lhs, const TimeSignature &rhs) {
                         return lhs.barIndex < rhs.barIndex;
                     });
    QList<TimeSignature> cleaned;
    cleaned.reserve(m_timeSignatures.size());
    for (const auto &signature : m_timeSignatures) {
        if (signature.barIndex < 0 || !signature.isValid()) {
            qWarning() << "Timeline: dropping invalid time signature at bar" << signature.barIndex
                       << signature.numerator << "/" << signature.denominator;
            continue;
        }
        if (!cleaned.isEmpty() && cleaned.last().barIndex == signature.barIndex) {
            qWarning() << "Timeline: dropping duplicate time signature at bar"
                       << signature.barIndex;
            continue;
        }
        cleaned.append(signature);
    }
    if (cleaned.isEmpty()) {
        cleaned.append(TimeSignature{});
    } else if (cleaned.first().barIndex != 0) {
        qWarning() << "Timeline: no time signature at bar 0; anchoring the earliest point";
        auto anchor = cleaned.first();
        anchor.barIndex = 0;
        cleaned.prepend(anchor);
    }
    m_timeSignatures = std::move(cleaned);
}

void Timeline::rebuildTempoCache() {
    m_msAtTempo.resize(m_tempos.size());
    m_tempoRunStart.resize(m_tempos.size());
    m_msAtTempo[0] = 0.0;
    m_tempoRunStart[0] = 0;
    for (qsizetype i = 1; i < m_tempos.size(); i++) {
        const auto &prev = m_tempos[i - 1];
        m_msAtTempo[i] = m_msAtTempo[i - 1] + (m_tempos[i].pos - prev.pos) * 60 / prev.value /
                                                  MusicTime::ticksPerQuarterNote * 1000;
        // A point that does not change the tempo continues the previous run, so
        // it shares the run's anchor for time conversion.
        m_tempoRunStart[i] =
            m_tempos[i].value == prev.value ? m_tempoRunStart[i - 1] : i;
    }
}

void Timeline::rebuildTimeSignatureCache() {
    m_tickAtSignature.resize(m_timeSignatures.size());
    m_tickAtSignature[0] = 0;
    for (qsizetype i = 1; i < m_timeSignatures.size(); i++) {
        const auto &prev = m_timeSignatures[i - 1];
        m_tickAtSignature[i] = m_tickAtSignature[i - 1] +
                               (m_timeSignatures[i].barIndex - prev.barIndex) * prev.ticksPerBar();
    }
}

qsizetype Timeline::tempoIndexAtTick(const double tick) const {
    const auto it =
        std::upper_bound(m_tempos.cbegin(), m_tempos.cend(), tick,
                         [](const double value, const Tempo &item) { return value < item.pos; });
    return qMax<qsizetype>(0, std::distance(m_tempos.cbegin(), it) - 1);
}

qsizetype Timeline::tempoIndexAtMs(const double ms) const {
    const auto it = std::upper_bound(m_msAtTempo.cbegin(), m_msAtTempo.cend(), ms);
    return qMax<qsizetype>(0, std::distance(m_msAtTempo.cbegin(), it) - 1);
}

qsizetype Timeline::signatureIndexAtBar(const int bar) const {
    const auto it = std::upper_bound(
        m_timeSignatures.cbegin(), m_timeSignatures.cend(), bar,
        [](const int value, const TimeSignature &item) { return value < item.barIndex; });
    return qMax<qsizetype>(0, std::distance(m_timeSignatures.cbegin(), it) - 1);
}

qsizetype Timeline::signatureIndexAtTick(const int tick) const {
    const auto it = std::upper_bound(m_tickAtSignature.cbegin(), m_tickAtSignature.cend(), tick);
    return qMax<qsizetype>(0, std::distance(m_tickAtSignature.cbegin(), it) - 1);
}
