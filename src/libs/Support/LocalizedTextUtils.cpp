#include "LocalizedTextUtils.h"

#include <IcuWrapper/IcuWrapper.h>

namespace lite::Support {

    QString lookupLocalizedText(const QMap<QString, QString> &localized, const QString &defaultText,
                                const QString &bcp47Locale) {
        if (localized.isEmpty())
            return defaultText;
        const auto hit = IcuWrapper::bestMatch(bcp47Locale, localized.keys());
        if (hit.isEmpty())
            return defaultText;
        return localized.value(hit, defaultText);
    }

    QString lookupLocalizedText(const QMap<QString, QString> &localized, const QString &defaultText,
                                const QStringList &bcp47LocaleCandidates) {
        if (localized.isEmpty())
            return defaultText;
        const auto keys = localized.keys();
        for (const auto &tag : bcp47LocaleCandidates) {
            const auto hit = IcuWrapper::bestMatch(tag, keys);
            if (!hit.isEmpty())
                return localized.value(hit, defaultText);
        }
        return defaultText;
    }

} // namespace lite::Support
