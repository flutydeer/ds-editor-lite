#ifndef LOGGINGBOOTSTRAP_H
#define LOGGINGBOOTSTRAP_H

#include "StartupArguments.h"

// Log directory setup and startup diagnostics (system / GPU info).
namespace LoggingBootstrap {

    // Creates the app data directory, configures log levels and logs
    // system + GPU diagnostics. Call after QApplication is constructed.
    // When `remoteLog` carries a target, every message is also mirrored to that
    // UDP endpoint so a build machine can watch the output live.
    void init(const StartupArguments::RemoteLogTarget &remoteLog = {});

} // namespace LoggingBootstrap

#endif // LOGGINGBOOTSTRAP_H
