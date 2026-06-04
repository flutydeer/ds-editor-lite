//
// Created by FlutyDeer on 2025/7/27.
//

#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#define packageManager PackageManager::instance()

#include <atomic>
#include <cstdint>
#include <mutex>

#include "Modules/PackageManager/Models/GetInstalledPackagesResult.h"
#include "Model/AppStatus/AppStatus.h"
#include "Utils/Expected.h"
#include "Utils/Singleton.h"

#include <QObject>
#include <QMutex>
#include <QReadWriteLock>

#include <synthrt/Core/SynthUnit.h>

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

private:
    static QString srtErrorToString(const srt::Error &error);
    bool initializeMetadataBackend(QString &error);

    enum class RefreshState: uint8_t {
        Idle = 0,
        Refreshing = 1,
    };
    std::once_flag m_initialized{};
    std::atomic<RefreshState> m_refreshState{RefreshState::Idle};
    QMutex m_metadataSynthUnitMutex;
    bool m_metadataSynthUnitInitialized = false;
    srt::SynthUnit m_metadataSynthUnit;
    mutable QReadWriteLock m_resultRwLock;
    GetInstalledPackagesResult m_result;
    QHash<SingerIdentifier, PackageInfo> m_packageLocator;
    QHash<SingerIdentifier, SingerInfo> m_singerLocator;
};

#endif //PACKAGEMANAGER_H
