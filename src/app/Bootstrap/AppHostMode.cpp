#include "AppHostMode.h"

QString appHostModeName(const AppHostMode mode) {
    switch (mode) {
        case AppHostMode::Gui:
            return QStringLiteral("gui");
        case AppHostMode::Headless:
            return QStringLiteral("headless");
    }
    return {};
}
