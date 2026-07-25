#include "TimeSignature.h"

TimeSignature::TimeSignature(const int num, const int deno) : numerator(num), denominator(deno) {
}

QJsonObject TimeSignature::serialize() const {
    return QJsonObject{
        {"pos",         pos        },
        {"numerator",   numerator  },
        {"denominator", denominator}
    };
}

bool TimeSignature::deserialize(const QJsonObject &obj) {
    pos = obj["pos"].toInt();
    numerator = obj["numerator"].toInt();
    denominator = obj["denominator"].toInt();
    return true;
}

bool operator==(const TimeSignature &lhs, const TimeSignature &rhs) {
    return lhs.pos == rhs.pos && lhs.numerator == rhs.numerator &&
           lhs.denominator == rhs.denominator;
}

bool operator!=(const TimeSignature &lhs, const TimeSignature &rhs) {
    return !(lhs == rhs);
}