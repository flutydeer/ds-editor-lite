#ifndef DS_EDITOR_LITE_MUSICTIMECONVERTER_H
#define DS_EDITOR_LITE_MUSICTIMECONVERTER_H

#include <QString>
#include <QStringView>

namespace MusicTimeConverter {

[[nodiscard]] double tickToMs(double tick, double tempo);
[[nodiscard]] double msToTick(double ms, double tempo);
[[nodiscard]] double tickToSec(double tick, double tempo);
[[nodiscard]] double secToTick(double sec, double tempo);
[[nodiscard]] QString getBarBeatTickTime(int ticks, int numerator, int denominator);

// Compact seconds-scale label for the timeline lanes, as opposed to the
// tick-based bar/beat readout: "m:ss.d" by default ("0:02.6"), "m:ss" when
// includeTenths is false. Minutes carry no leading zero and the fraction is a
// single tenth of a second, so the text stays short enough for a 28px row.
// Negative or non-finite input is clamped to zero.
[[nodiscard]] QString formatSecondsLabel(double milliseconds, bool includeTenths = true);

// Full transport readout used by the title bar time code: "mm:ss.mmm"
// ("01:14.514"). Minutes are padded to two digits and the fraction keeps all
// three millisecond digits, because the transport is the precise entry point.
// Negative or non-finite input is clamped to zero.
[[nodiscard]] QString formatTimecode(double milliseconds);

// Parses either shape back: "mm:ss.mmm", "m:ss.d", "m:ss" or "ss.d" (one to
// three fractional digits, scaled to milliseconds). Seconds may exceed 59 so
// that coarse entries such as "0:75.0" are accepted. Returns false for
// malformed text or an empty input, leaving *milliseconds untouched. Both
// formatters stay in the C locale (see the localization guide): these are
// machine-shaped readouts, not localized numbers.
[[nodiscard]] bool parseMinSec(QStringView text, double *milliseconds);

} // namespace MusicTimeConverter

#endif // DS_EDITOR_LITE_MUSICTIMECONVERTER_H
