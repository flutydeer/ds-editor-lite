//
// Created by FlutyDeer on 2025/7/27.
//

#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#include "Model/AppStatus/AppStatus.h"
#include "Model/Package/Package.h"
#include "Utils/Expected.h"
#include "Utils/Singleton.h"

#include <QObject>

class PackageManager final : public QObject, public Singleton<PackageManager> {
    Q_OBJECT

public:
    enum GetInstalledPackagesErrorType {
        InferEngineNotInitialized,
        Unknown
    };

    class GetInstalledPackagesListError {
    public:
        GetInstalledPackagesErrorType type = Unknown;
        QString message;
    };

    PackageManager();
    [[nodiscard]] Expected<QList<Package>, GetInstalledPackagesListError> getInstalledPackages() const;

private slots:
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
};

#endif //PACKAGEMANAGER_H