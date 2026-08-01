#ifndef GETINSTALLEDPACKAGESTASK_H
#define GETINSTALLEDPACKAGESTASK_H

#include <lite/PackageManager/Models/GetInstalledPackagesResult.h>
#include <lite/Tasking/Task.h>
#include <lite/ADT/Expected.h>

#include <QStringList>

class GetInstalledPackagesTask final : public Task {
    Q_OBJECT

 public:
    explicit GetInstalledPackagesTask(QStringList searchPaths);

    Expected<GetInstalledPackagesResult, GetInstalledPackagesError> result;

 private:
    void runTask() override;

    QStringList m_searchPaths;
};



#endif //GETINSTALLEDPACKAGESTASK_H
