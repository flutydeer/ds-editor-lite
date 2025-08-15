#include "VersionUtils.h"

namespace VersionUtils {
    stdc::VersionNumber qt_to_stdc(const QVersionNumber &version) {
        const auto count = version.segmentCount();
        int segments[4] = {0, 0, 0, 0};
        for (qsizetype i = 0; i < count; ++i) {
            segments[i] = version.segmentAt(i);
        }
        return stdc::VersionNumber(segments[0], segments[1], segments[2], segments[3]);
    }

    QVersionNumber stdc_to_qt(const stdc::VersionNumber &version) {
        QList<int> arr;
        arr.reserve(4);
        arr.push_back(version.major());
        arr.push_back(version.minor());
        const auto patch = version.patch();
        const auto tweak = version.tweak();
        if (patch) {
            arr.push_back(patch);
        } else {
            if (tweak) {
                arr.push_back(0);
            }
        }
        if (tweak) {
            arr.push_back(tweak);
        }

        return QVersionNumber(std::move(arr));
    }
}