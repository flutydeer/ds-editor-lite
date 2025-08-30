//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef GETINSTALLEDPACKAGESRESULT_H
#define GETINSTALLEDPACKAGESRESULT_H

#include "PackageInfo.h"

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
        explicit FailedPackage(QString path = {}, QString reason = {})
            : path(std::move(path)), reason(std::move(reason)) {
        }

        QString path;
        QString reason;
    };

    QList<PackageInfo> successfulPackages;
    QList<FailedPackage> failedPackages;
};

#endif //GETINSTALLEDPACKAGESRESULT_H
