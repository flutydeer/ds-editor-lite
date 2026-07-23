#ifndef VERSION_UTILS_H
#define VERSION_UTILS_H

#include <QVersionNumber>
#include <stdcorelib/support/versionnumber.h>

namespace VersionUtils {
    stdc::VersionNumber qt_to_stdc(const QVersionNumber &version);
    QVersionNumber stdc_to_qt(const stdc::VersionNumber &version);
}

#endif // VERSION_UTILS_H