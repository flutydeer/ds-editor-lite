//
// Created by FlutyDeer on 2025/7/27.
//

#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#define packageManager PackageManager::instance()

#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "Modules/PackageManager/Models/GetInstalledPackagesResult.h"
#include "Model/AppStatus/AppStatus.h"
#include "Utils/Expected.h"
#include "Utils/Singleton.h"

#include <QObject>
#include <QMutex>
#include <QReadWriteLock>

namespace srt::core {
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

private:
    static QString srtErrorToString(const srt::core::Error &error);

    std::once_flag m_initialized{};
    mutable std::mutex m_refreshMutex;
    std::condition_variable m_refreshCompleted;
    bool m_refreshing = false;
    Expected<GetInstalledPackagesResult, GetInstalledPackagesError> m_lastRefreshResult;
    mutable QReadWriteLock m_resultRwLock;
    GetInstalledPackagesResult m_result;
    uint64_t m_catalogGeneration = 0;
    QHash<SingerIdentifier, PackageInfo> m_packageLocator;
    QHash<SingerIdentifier, SingerInfo> m_singerLocator;
};

#endif // PACKAGEMANAGER_H
