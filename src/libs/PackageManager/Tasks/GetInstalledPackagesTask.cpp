//
// Created by FlutyDeer on 2025/7/31.
//

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
    // TODO 获取进度？
    result = packageManager->refreshInstalledPackages(m_searchPaths);
}