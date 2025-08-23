//
// Created by FlutyDeer on 2025/7/31.
//

#include "GetInstalledPackagesTask.h"

#include "Modules/PackageManager/PackageManager.h"
#include "Modules/Task/Task.h"

GetInstalledPackagesTask::GetInstalledPackagesTask() {
    TaskStatus status;
    status.title = tr("Get Installed Packages");
    status.isIndetermine = true;
    setStatus(status);
}

void GetInstalledPackagesTask::runTask() {
    // TODO 获取进度？
    result = packageManager->refreshInstalledPackages();
}