#ifndef APPLICATIONLOCALE_H
#define APPLICATIONLOCALE_H

#include <QLocale>

namespace ApplicationLocale {

    [[nodiscard]] QLocale configuredLocale(const QLocale &systemLocale);
    void initialize();

} // namespace ApplicationLocale

#endif // APPLICATIONLOCALE_H
