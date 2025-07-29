//
// Created by FlutyDeer on 2025/7/27.
//

#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#include "Model/AppStatus/AppStatus.h"
#include "Utils/Expected.h"
#include "Utils/Singleton.h"

#include <QObject>

namespace srt {
    class PackageRef;
    class Error;
}

class PackageManager final : public QObject, public Singleton<PackageManager> {
    Q_OBJECT

public:
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

    PackageManager();

    [[nodiscard]]
    static Expected<GetInstalledPackagesResult, GetInstalledPackagesError> getInstalledPackages();

private slots:
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);

private:
    static QString srtErrorToString(const srt::Error &error);
};

#endif //PACKAGEMANAGER_H