#include "TimecodeFormatUtils.h"

#include <lite/MusicBase/MusicTime.h>
#include <lite/MusicBase/MusicTimeConverter.h>
#include <lite/MusicBase/Timeline.h>

#include <limits>
#include <cmath>

namespace {
    // Separator between the two readouts of the combined format. The popup and
    // the title bar both render and strip exactly this sequence.
    QString combinedSeparator() {
        return QStringLiteral("·");
    }

    QString combinedText(const QString &barBeatTick, const QString &secondsText) {
        return barBeatTick + QStringLiteral(" ") + combinedSeparator() + QStringLiteral(" ") +
               secondsText;
    }

    bool parseBarBeatTick(const Timeline &timeline, const QString &text, int *tick) {
        bool ok = false;
        const auto time = MusicTime::fromString(text, &ok);
        if (!ok)
            return false;
        const auto value = timeline.timeToTick(time);
        if (value < 0)
            return false;
        *tick = value;
        return true;
    }

    bool parseSeconds(const Timeline &timeline, const QString &text, int *tick) {
        double milliseconds = 0.0;
        if (!MusicTimeConverter::parseMinSec(QStringView(text), &milliseconds))
            return false;
        const auto value = timeline.msToTick(milliseconds);
        if (!std::isfinite(value) || value < 0.0 ||
            value > static_cast<double>(std::numeric_limits<int>::max()))
            return false;
        *tick = static_cast<int>(value + 0.5);
        return true;
    }
} // namespace

namespace TimecodeFormatUtils {

    QString formatPosition(const Timeline &timeline, const int tick,
                           const AppearanceOption::TimecodeFormat format) {
        const auto position = qMax(0, tick);
        const auto barBeatTick = [&] { return timeline.getBarBeatTickTime(position); };
        const auto secondsText = [&] {
            return MusicTimeConverter::formatTimecode(timeline.tickToMs(position));
        };
        switch (format) {
            case AppearanceOption::TimecodeFormat::Time:
                return secondsText();
            case AppearanceOption::TimecodeFormat::Both:
                return combinedText(barBeatTick(), secondsText());
            case AppearanceOption::TimecodeFormat::BarBeatTick:
                break;
        }
        return barBeatTick();
    }

    bool parsePosition(const Timeline &timeline, const QStringView text,
                       const AppearanceOption::TimecodeFormat format, int *tick) {
        auto normalized = text.toString().trimmed();
        if (normalized.isEmpty())
            return false;

        // The combined readout is two values; the musical half is authoritative
        // because it is the one the user edits against the grid.
        if (format == AppearanceOption::TimecodeFormat::Both) {
            const auto separatorIndex = normalized.indexOf(combinedSeparator());
            if (separatorIndex >= 0)
                normalized = normalized.left(separatorIndex).trimmed();
        }
        if (normalized.isEmpty())
            return false;

        if (format == AppearanceOption::TimecodeFormat::Time)
            return parseSeconds(timeline, normalized, tick);
        if (format == AppearanceOption::TimecodeFormat::Both && normalized.contains(QLatin1Char('.')))
            return parseSeconds(timeline, normalized, tick);
        return parseBarBeatTick(timeline, normalized, tick);
    }

} // namespace TimecodeFormatUtils
