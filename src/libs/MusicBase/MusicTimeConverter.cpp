#include "MusicTimeConverter.h"
#include "MusicTime.h"

#include <QStringList>

#include <cmath>
#include <optional>

namespace {
    // Keeps hostile input such as "99999999999999999999:00" from overflowing the
    // qint64 accumulation below.
    constexpr qint64 kMaxMinutes = 100000000;

    std::optional<qint64> parseUnsigned(QStringView text) {
        if (text.isEmpty())
            return std::nullopt;
        for (const auto ch : text) {
            if (!ch.isDigit())
                return std::nullopt;
        }
        bool ok = false;
        const auto value = text.toLongLong(&ok);
        if (!ok || value < 0)
            return std::nullopt;
        return value;
    }
} // namespace

namespace MusicTimeConverter {

    double tickToMs(const double tick, const double tempo) {
        return tick * 60 / tempo / MusicTime::ticksPerQuarterNote * 1000;
    }

    double msToTick(const double ms, const double tempo) {
        return ms * MusicTime::ticksPerQuarterNote * tempo / 60000;
    }

    double tickToSec(const double tick, const double tempo) {
        return tickToMs(tick, tempo) / 1000.0;
    }

    double secToTick(const double sec, const double tempo) {
        return msToTick(sec * 1000.0, tempo);
    }

    QString getBarBeatTickTime(const int ticks, const int numerator, const int denominator) {
        const int barTicks = MusicTime::ticksPerWholeNote * numerator / denominator;
        const int beatTicks = MusicTime::ticksPerWholeNote / denominator;
        const auto bar = ticks / barTicks + 1;
        const auto beat = ticks % barTicks / beatTicks + 1;
        const auto tick = ticks % barTicks % beatTicks;
        auto str = QString::asprintf("%03d", bar) + ":" + QString::asprintf("%02d", beat) + ":" +
                   QString::asprintf("%03d", tick);
        return str;
    }

    QString formatSecondsLabel(const double milliseconds, const bool includeTenths) {
        const auto valid = std::isfinite(milliseconds) && milliseconds > 0.0;
        // Round once, to tenths, so both spellings describe the same instant:
        // 74.514 s stays 1:14 (with the tenths dropped), never 1:15.
        const auto tenths = valid ? qRound64(milliseconds / 100.0) : 0;
        if (!includeTenths) {
            return QString::asprintf("%lld:%02lld", static_cast<long long>(tenths / 600),
                                     static_cast<long long>(tenths % 600 / 10));
        }
        return QString::asprintf("%lld:%02lld.%lld", static_cast<long long>(tenths / 600),
                                 static_cast<long long>(tenths % 600 / 10),
                                 static_cast<long long>(tenths % 10));
    }

    QString formatTimecode(const double milliseconds) {
        const auto value =
            std::isfinite(milliseconds) && milliseconds > 0.0 ? qRound64(milliseconds) : 0;
        return QString::asprintf("%02lld:%02lld.%03lld", static_cast<long long>(value / 60000),
                                 static_cast<long long>(value % 60000 / 1000),
                                 static_cast<long long>(value % 1000));
    }

    bool parseMinSec(const QStringView text, double *milliseconds) {
        auto normalized = text.toString().trimmed();
        if (normalized.isEmpty())
            return false;
        // Accept the full-width colon produced by IME input.
        normalized.replace(QChar(0xFF1A), QLatin1Char(':'));

        const auto parts = normalized.split(QLatin1Char(':'));
        if (parts.isEmpty() || parts.size() > 2)
            return false;

        qint64 minutes = 0;
        if (parts.size() == 2) {
            const auto parsedMinutes = parseUnsigned(parts.at(0).trimmed());
            if (!parsedMinutes || *parsedMinutes > kMaxMinutes)
                return false;
            minutes = *parsedMinutes;
        }

        const auto secondsText = parts.constLast().trimmed();
        const auto dotIndex = secondsText.indexOf(QLatin1Char('.'));
        const auto secondsPart =
            dotIndex < 0 ? secondsText : secondsText.left(dotIndex);
        const auto parsedSeconds = parseUnsigned(secondsPart);
        if (!parsedSeconds || *parsedSeconds > kMaxMinutes * 60)
            return false;

        qint64 fraction = 0;
        if (dotIndex >= 0) {
            const auto fractionText = secondsText.mid(dotIndex + 1);
            if (fractionText.isEmpty() || fractionText.size() > 3)
                return false;
            const auto parsedFraction = parseUnsigned(fractionText);
            if (!parsedFraction)
                return false;
            fraction = *parsedFraction;
            for (auto digits = fractionText.size(); digits < 3; ++digits)
                fraction *= 10;
        }

        if (milliseconds)
            *milliseconds = static_cast<double>((minutes * 60 + *parsedSeconds) * 1000 + fraction);
        return true;
    }

} // namespace MusicTimeConverter
