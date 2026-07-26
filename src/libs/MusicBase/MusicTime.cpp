#include "MusicTime.h"

#include <QStringList>

MusicTime MusicTime::fromString(QStringView str, bool *ok) {
    bool okStub = false;
    if (!ok)
        ok = &okStub;
    *ok = false;

    QString normalized = str.toString();
    normalized.replace(QChar(0xFF1A), QLatin1Char(':'));
    const auto parts = normalized.split(QLatin1Char(':'));
    if (parts.isEmpty() || parts.size() > 3)
        return {-1, -1, -1};

    int values[3] = {1, 1, 0}; // 1-based measure/beat defaults, tick 0
    bool anyComponent = false;
    for (qsizetype i = 0; i < parts.size(); i++) {
        const auto trimmed = parts[i].trimmed();
        if (trimmed.isEmpty())
            continue;
        bool numberOk = false;
        values[i] = trimmed.toInt(&numberOk);
        if (!numberOk)
            return {-1, -1, -1};
        anyComponent = true;
    }
    if (!anyComponent)
        return {-1, -1, -1};

    *ok = true;
    return {values[0] - 1, values[1] - 1, values[2]};
}

QString MusicTime::toString() const {
    return QString::asprintf("%03d:%02d:%03d", measure + 1, beat + 1, tick);
}
