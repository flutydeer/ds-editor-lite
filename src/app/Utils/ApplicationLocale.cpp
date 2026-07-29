#include "ApplicationLocale.h"

namespace ApplicationLocale {

    QLocale configuredLocale(const QLocale &systemLocale) {
        auto locale = systemLocale;
        locale.setNumberOptions(locale.numberOptions() | QLocale::OmitGroupSeparator);
        return locale;
    }

    void initialize() {
        QLocale::setDefault(configuredLocale(QLocale::system()));
    }

} // namespace ApplicationLocale
