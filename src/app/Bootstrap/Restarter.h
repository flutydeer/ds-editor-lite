#ifndef RESTARTER_H
#define RESTARTER_H

#include <QString>

// Relaunches the application after the event loop exits if the "restart"
// property has been set on the application object.
class Restarter {
public:
    explicit Restarter(const QString &workingDir);

    /// Waits, for a bounded time, until the process this one was started to replace has exited.
    ///
    /// Called before the single-instance lock is taken. Does nothing when this process was not
    /// started by a restart.
    static void waitForPredecessor();

    int restartOrExit(int exitCode) const;
    int restart(int exitCode) const;

private:
    QString m_workingDir;
};

#endif // RESTARTER_H
