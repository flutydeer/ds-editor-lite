#include <lite/ProjectModel/AppModel/LoopSettings.h>
#include <lite/MusicBase/MusicTime.h>

LoopSettings::LoopSettings(bool enabled, int start, int length)
    : enabled(enabled), start(start), length(length) {
}

QJsonObject LoopSettings::serialize() const {
    return QJsonObject{
        {"enabled", enabled},
        {"start",   start  },
        {"length",  length }
    };
}

bool LoopSettings::deserialize(const QJsonObject &obj) {
    enabled = obj["enabled"].toBool(false);
    start = obj["start"].toInt(0);
    length = obj["length"].toInt(MusicTime::ticksPerWholeNote);
    return true;
}

bool operator==(const LoopSettings &lhs, const LoopSettings &rhs) {
    return lhs.enabled == rhs.enabled && lhs.start == rhs.start && lhs.length == rhs.length;
}

bool operator!=(const LoopSettings &lhs, const LoopSettings &rhs) {
    return !(lhs == rhs);
}
