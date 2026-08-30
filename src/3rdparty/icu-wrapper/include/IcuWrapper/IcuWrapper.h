#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QtGlobal>

#if defined(ICU_WRAPPER_BUILD_SHARED)
#  define ICU_WRAPPER_EXPORT Q_DECL_EXPORT
#elif defined(ICU_WRAPPER_SHARED)
#  define ICU_WRAPPER_EXPORT Q_DECL_IMPORT
#else
#  define ICU_WRAPPER_EXPORT
#endif

class ICU_WRAPPER_EXPORT IcuWrapper {
public:
    // Returns the original entry from available, or an empty string when no
    // meaningful language match exists.
    static QString bestMatch(const QString &requested, const QStringList &available);
};
