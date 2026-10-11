#include "Utils/AppDumpDirectory.h"

#include "Bootstrap/AppDataPaths.h"

#include <QDir>
#include <QMCore/qmsystem.h>

namespace AppDumpDirectory {

    QString resolveDumpDirectory() {
        return AppDataPaths::applicationData() + QStringLiteral("/Dumps");
    }

    void openDumpDirectory() {
        const auto dir = resolveDumpDirectory();
        QDir().mkpath(dir);
        QM::reveal(dir);
    }

} // namespace AppDumpDirectory
