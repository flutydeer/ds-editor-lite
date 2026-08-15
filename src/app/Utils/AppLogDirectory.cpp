#include "Utils/AppLogDirectory.h"

#include <lite/Support/Log.h>

#include <QDir>
#include <QMCore/qmsystem.h>
#include <QStandardPaths>

namespace AppLogDirectory {

    QString resolveLogDirectory() {
        auto dir = Log::logDirectory();
        if (dir.isEmpty())
            dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                  QStringLiteral("/Logs");
        return dir;
    }

    void openLogDirectory() {
        const auto dir = resolveLogDirectory();
        QDir().mkpath(dir);
        QM::reveal(dir);
    }

} // namespace AppLogDirectory
