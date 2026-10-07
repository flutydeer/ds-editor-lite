#include "Utils/AppDumpDirectory.h"

#include <QDir>
#include <QMCore/qmsystem.h>
#include <QStandardPaths>

namespace AppDumpDirectory {

    QString resolveDumpDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
               QStringLiteral("/Dumps");
    }

    void openDumpDirectory() {
        const auto dir = resolveDumpDirectory();
        QDir().mkpath(dir);
        QM::reveal(dir);
    }

} // namespace AppDumpDirectory
