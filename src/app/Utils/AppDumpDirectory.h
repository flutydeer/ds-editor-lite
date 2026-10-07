#ifndef APPDUMPDIRECTORY_H
#define APPDUMPDIRECTORY_H

#include <QString>

namespace AppDumpDirectory {
    /// Returns the directory where crash minidumps are written. Must stay in
    /// sync with the path handed to QBreakpadHandler::setDumpPath in
    /// Bootstrap/CrashHandler.cpp.
    QString resolveDumpDirectory();

    /// Creates the dump directory if needed and reveals it in the file
    /// explorer. The directory stays empty on builds without crash reporting.
    void openDumpDirectory();

} // namespace AppDumpDirectory

#endif // APPDUMPDIRECTORY_H
