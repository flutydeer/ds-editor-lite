#ifndef APPHOSTMODE_H
#define APPHOSTMODE_H

#include <QString>

enum class AppHostMode {
    Gui,
    Headless,
};

[[nodiscard]] QString appHostModeName(AppHostMode mode);

#endif // APPHOSTMODE_H
