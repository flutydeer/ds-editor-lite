#ifndef APPLOGDIRECTORY_H
#define APPLOGDIRECTORY_H

#include <QString>

namespace AppLogDirectory {
    /// Returns the directory used for file logs: the one configured via
    /// Log::setLogDirectory (packaged builds), or AppData/Logs as fallback.
    /// The returned directory is NOT created by this function.
    QString resolveLogDirectory();

    /// Creates the log directory if needed and reveals it in the file
    /// explorer. Safe to call on any build (creates the dir only on demand).
    void openLogDirectory();

} // namespace AppLogDirectory

#endif // APPLOGDIRECTORY_H
