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
    // Reuse the engine's initial scan for the first package listing.
    result = packageManager->refreshInstalledPackages(m_searchPaths, {},
                                                      PackageManager::ScanMode::ReuseLoaded);
}
