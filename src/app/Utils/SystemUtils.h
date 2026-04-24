//
// Created by fluty on 24-9-3.
//

#ifndef SYSTEMUTILS_H
#define SYSTEMUTILS_H

#include <QString>
#include <QSysInfo>
#include <QVersionNumber>

class SystemUtils {
public:
    enum class SystemProductType { Windows, MacOS, Linux };
    static SystemProductType productType();
    static bool isWindows();
    static bool isWindows11();
};

inline SystemUtils::SystemProductType SystemUtils::productType() {
    if (QSysInfo::productType() == "windows")
        return SystemProductType::Windows;
    if (QSysInfo::productType() == "macos")
        return SystemProductType::MacOS;
    return SystemProductType::Linux;
}

inline bool SystemUtils::isWindows() {
    return productType() == SystemProductType::Windows;
}

inline bool SystemUtils::isWindows11() {
#ifdef Q_OS_WIN
    auto version = QVersionNumber::fromString(QSysInfo::productVersion());
    return version >= QVersionNumber(11);
#else
    return false;
#endif
}

#endif // SYSTEMUTILS_H
