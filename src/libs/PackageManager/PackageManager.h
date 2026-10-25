#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#define packageManager PackageManager::instance()

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>

#include <lite/PackageManager/Models/GetInstalledPackagesResult.h>
#include <lite/ADT/Expected.h>
#include <lite/Core/Singleton.h>

#include <QObject>
#include <QMutex>
#include <QReadWriteLock>
#include <QStringList>

class PackageManager final : public QObject {
    Q_OBJECT

private:
    explicit PackageManager(QObject *parent = nullptr);
    ~PackageManager() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(PackageManager)
    Q_DISABLE_COPY_MOVE(PackageManager)

public:
    enum class ModuleStatus { Loading, Ready, Error };
    using RefreshCommitGate = std::function<bool()>;

    enum class ScanMode {
        /// Read changed package files through a fresh runtime catalog.
        Reload,
        /// Reuse the packages already loaded during initialization.
        ReuseLoaded,
    };

    // searchPaths: directories to scan for packages. Supplied by the app (from
    // its settings) so the library does not depend on AppOptions.
    void initialize(const QStringList &searchPaths);

    [[nodiscard]]
    Expected<GetInstalledPackagesResult, GetInstalledPackagesError>
        refreshInstalledPackages(const QStringList &searchPaths, RefreshCommitGate commitGate = {},
                                 ScanMode mode = ScanMode::Reload);

    GetInstalledPackagesResult installedPackages() const;
    PackageInfo findPackageByIdentifier(const SingerIdentifier &identifier) const;
    SingerInfo findSingerByIdentifier(const SingerIdentifier &identifier) const;

Q_SIGNALS:
    void packagesRefreshed(QList<PackageInfo> packages);
    // Package-scan lifecycle. The app maps this to AppStatus::packageModuleStatus
    // so the library stays free of AppStatus.
    void moduleStatusChanged(PackageManager::ModuleStatus status);

private:
    std::once_flag m_initialized{};
    mutable std::mutex m_refreshMutex;
    std::condition_variable m_refreshCompleted;
    bool m_refreshing = false;
    QStringList m_refreshSearchPaths;
    ScanMode m_refreshMode = ScanMode::Reload;
    bool m_lastRefreshCommitRejected = false;
    Expected<GetInstalledPackagesResult, GetInstalledPackagesError> m_lastRefreshResult;
    mutable QReadWriteLock m_resultRwLock;
    GetInstalledPackagesResult m_result;
    uint64_t m_catalogGeneration = 0;
    QHash<SingerIdentifier, PackageInfo> m_packageLocator;
    QHash<SingerIdentifier, SingerInfo> m_singerLocator;
};

#endif // PACKAGEMANAGER_H
