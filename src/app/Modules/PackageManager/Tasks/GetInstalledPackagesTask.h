//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef GETINSTALLEDPACKAGESTASK_H
#define GETINSTALLEDPACKAGESTASK_H

#include "Modules/PackageManager/Models/GetInstalledPackagesResult.h"
#include "Modules/Task/Task.h"
#include "Utils/Expected.h"

class GetInstalledPackagesTask final : public Task {
    Q_OBJECT

 public:
    explicit GetInstalledPackagesTask();

    Expected<GetInstalledPackagesResult, GetInstalledPackagesError> result;

 private:
    void runTask() override;
};



#endif //GETINSTALLEDPACKAGESTASK_H
