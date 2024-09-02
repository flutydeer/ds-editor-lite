//
// Created by fluty on 24-9-3.
//

#ifndef SYSTEMUTILS_H
#define SYSTEMUTILS_H

#include <QString>
#include <QSysInfo>

class SystemUtils {
public:
    enum class SystemProductType { Windows, MacOS, Linux };
    static SystemProductType productType();
};

inline SystemUtils::SystemProductType SystemUtils::productType() {
    if (QSysInfo::productType() == "windows")
        return SystemProductType::Windows;
    if (QSysInfo::productType() == "macos")
        return SystemProductType::MacOS;
    return SystemProductType::Linux;
}

#endif // SYSTEMUTILS_H
