#include "TimeSignature.h"
#include "MusicTime.h"

TimeSignature::TimeSignature(const int num, const int deno) : numerator(num), denominator(deno) {
}

TimeSignature::TimeSignature(const int barIndex, const int num, const int deno)
    : barIndex(barIndex), numerator(num), denominator(deno) {
}

QJsonObject TimeSignature::serialize() const {
    // The JSON key stays "pos" for compatibility with existing project files
    // and inference cache signatures.
    return QJsonObject{
        {"pos",         barIndex   },
        {"numerator",   numerator  },
        {"denominator", denominator}
    };
}

bool TimeSignature::deserialize(const QJsonObject &obj) {
    barIndex = obj["pos"].toInt();
    numerator = obj["numerator"].toInt();
    denominator = obj["denominator"].toInt();
    return true;
}

bool TimeSignature::isValid() const {
    return numerator > 0 && denominator > 0;
}

int TimeSignature::ticksPerBar() const {
    // A bar is exactly `numerator` beats; deriving it from ticksPerBeat keeps
    // the two consistent even for denominators that do not divide the whole
    // note evenly.
    return ticksPerBeat() * (numerator > 0 ? numerator : 4);
}

int TimeSignature::ticksPerBeat() const {
    const int deno = denominator > 0 ? denominator : 4;
    const int ticks = MusicTime::ticksPerWholeNote / deno;
    return ticks > 0 ? ticks : 1;
}

bool operator==(const TimeSignature &lhs, const TimeSignature &rhs) {
    return lhs.barIndex == rhs.barIndex && lhs.numerator == rhs.numerator &&
           lhs.denominator == rhs.denominator;
}

bool operator!=(const TimeSignature &lhs, const TimeSignature &rhs) {
    return !(lhs == rhs);
}
