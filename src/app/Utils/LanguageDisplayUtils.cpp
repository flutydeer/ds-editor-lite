#include "Utils/LanguageDisplayUtils.h"

#include <QCoreApplication>

namespace LanguageDisplayUtils {
    QString displayName(const QString &languageId) {
        if (languageId.isEmpty())
            return {};
        if (languageId == QStringLiteral("cmn"))
            return QCoreApplication::translate("LanguageDisplayUtils", "Mandarin");
        if (languageId == QStringLiteral("eng"))
            return QCoreApplication::translate("LanguageDisplayUtils", "English");
        if (languageId == QStringLiteral("jpn"))
            return QCoreApplication::translate("LanguageDisplayUtils", "Japanese");
        if (languageId == QStringLiteral("yue"))
            return QCoreApplication::translate("LanguageDisplayUtils", "Cantonese");
        if (languageId == QStringLiteral("kor"))
            return QCoreApplication::translate("LanguageDisplayUtils", "Korean");
        return languageId;
    }
}
