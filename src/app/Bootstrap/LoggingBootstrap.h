#ifndef LOGGINGBOOTSTRAP_H
#define LOGGINGBOOTSTRAP_H

// Log directory setup and startup diagnostics (system / GPU info).
namespace LoggingBootstrap {

    // Creates the app data directory, configures log levels and logs
    // system + GPU diagnostics. Call after QApplication is constructed.
    void init();

} // namespace LoggingBootstrap

#endif // LOGGINGBOOTSTRAP_H
