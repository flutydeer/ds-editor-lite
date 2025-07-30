//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef GETINSTALLEDPACKAGESRESULT_H
#define GETINSTALLEDPACKAGESRESULT_H

#include "synthrt/Core/PackageRef.h"

#include <QList>
#include <QString>

enum GetInstalledPackagesErrorType {
    InferEngineNotInitialized,
    Unknown
};

struct GetInstalledPackagesError {
    GetInstalledPackagesErrorType type = Unknown;
    QString message;
};

struct GetInstalledPackagesResult {
    struct FailedPackage {
        QString path;
        QString reason;
    };

    QList<srt::PackageRef> successfulPackages;
    QList<FailedPackage> failedPackages;
};

#endif //GETINSTALLEDPACKAGESRESULT_H
