//
// Created by FlutyDeer on 2025/7/27.
//

#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#define packageManager PackageManager::instance()

#include <cstdint>
#include <mutex>

#include "Modules/PackageManager/Models/GetInstalledPackagesResult.h"
#include "Model/AppStatus/AppStatus.h"
#include "Utils/Expected.h"
#include "Utils/Singleton.h"

#include <QObject>
#include <QReadWriteLock>

namespace srt {
    class PackageRef;
    class Error;
}

class PackageManager final : public QObject {
    Q_OBJECT

private:
    explicit PackageManager(QObject *parent = nullptr);
    ~PackageManager() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(PackageManager)
    Q_DISABLE_COPY_MOVE(PackageManager)

public:
    void initialize();

    [[nodiscard]]
    Expected<GetInstalledPackagesResult, GetInstalledPackagesError> refreshInstalledPackages();

    GetInstalledPackagesResult installedPackages() const;
    PackageInfo findPackageByIdentifier(const SingerIdentifier &identifier) const;
    SingerInfo findSingerByIdentifier(const SingerIdentifier &identifier) const;

Q_SIGNALS:
    void packagesRefreshed(QList<PackageInfo> packages);

private Q_SLOTS:
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);

private:
    static QString srtErrorToString(const srt::Error &error);

    enum class RefreshState: uint8_t {
        Idle = 0,
        Refreshing = 1,
    };
    std::once_flag m_initialized{};
    std::atomic<RefreshState> m_refreshState{RefreshState::Idle};
    mutable QReadWriteLock m_resultRwLock;
    GetInstalledPackagesResult m_result;
    QHash<SingerIdentifier, PackageInfo> m_packageLocator;
    QHash<SingerIdentifier, SingerInfo> m_singerLocator;
};

#endif //PACKAGEMANAGER_H
