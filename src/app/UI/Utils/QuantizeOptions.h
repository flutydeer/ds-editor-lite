#ifndef QUANTIZEOPTIONS_H
#define QUANTIZEOPTIONS_H

#include <QList>
#include <QStringList>

namespace QuantizeOptions {

    // "Divisions per whole note" scheme shared by the piano roll toolbar combo
    // and the Quantize dialog. Ticks = 1920 / q (TimelineSnapUtils::quantizeToTicks).
    // Any divisor of 1920 works, so tuplets (q multiple of 3) are exact.
    inline const QStringList &strings() {
        static const QStringList kStrings = {"1/2",   "1/4",   "1/8",   "1/16",  "1/32",
                                             "1/64",  "1/128", "1/2T",  "1/4T",  "1/8T",
                                             "1/16T", "1/32T", "1/64T", "1/128T"};
        return kStrings;
    }

    inline const QList<int> &values() {
        static const QList<int> kValues = {2, 4, 8, 16, 32, 64, 128, 3, 6, 12, 24, 48, 96, 192};
        return kValues;
    }

    inline int indexOf(int quantize) {
        const int index = values().indexOf(quantize);
        return index >= 0 ? index : values().indexOf(16);
    }

} // namespace QuantizeOptions

#endif // QUANTIZEOPTIONS_H
