#include "Utils/AppLogDirectory.h"
#include "Bootstrap/AppDataPaths.h"

#include <lite/Support/Log.h>

#include <QDir>
#include <QMCore/qmsystem.h>
#include <QStandardPaths>

namespace AppLogDirectory {

    QString resolveLogDirectory() {
        auto dir = Log::logDirectory();
        if (dir.isEmpty())
            dir = AppDataPaths::applicationData() + QStringLiteral("/Logs");
        return dir;
    }

    void openLogDirectory() {
        const auto dir = resolveLogDirectory();
        QDir().mkpath(dir);
        QM::reveal(dir);
    }

} // namespace AppLogDirectory
