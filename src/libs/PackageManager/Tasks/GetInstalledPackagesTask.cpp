#include "GetInstalledPackagesTask.h"

#include <lite/PackageManager/PackageManager.h>
#include <lite/Tasking/Task.h>

GetInstalledPackagesTask::GetInstalledPackagesTask(QStringList searchPaths)
    : m_searchPaths(std::move(searchPaths)) {
    TaskStatus status;
    status.title = tr("Get Installed Packages");
    status.isIndetermine = true;
    setStatus(status);
}

void GetInstalledPackagesTask::runTask() {
    // First listing after startup. The engine scanned the same paths during initialization, so
    // the packages that it loaded are reused instead of being loaded a second time.
    result = packageManager->refreshInstalledPackages(m_searchPaths, {},
                                                      PackageManager::ScanMode::ReuseLoaded);
}
