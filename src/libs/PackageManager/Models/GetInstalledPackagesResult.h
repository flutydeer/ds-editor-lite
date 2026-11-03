#ifndef GETINSTALLEDPACKAGESRESULT_H
#define GETINSTALLEDPACKAGESRESULT_H

#include "PackageInfo.h"

#include <QList>
#include <QString>
#include <QHash>
#include <QSet>

enum class GetInstalledPackagesErrorType {
    MetadataBackendNotInitialized,
    /// The caller declined publication of the scan result.
    CommitRejected,
    Unknown
};

struct GetInstalledPackagesError {
    GetInstalledPackagesErrorType type = GetInstalledPackagesErrorType::Unknown;
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
    QHash<SingerIdentifier, QSet<QString>> convertibleLanguages;
};

#endif //GETINSTALLEDPACKAGESRESULT_H
