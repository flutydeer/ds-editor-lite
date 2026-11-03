#ifndef RESTARTER_H
#define RESTARTER_H

#include <QString>

// Relaunches the application after the event loop exits if the "restart"
// property has been set on the application object.
class Restarter {
public:
    explicit Restarter(const QString &workingDir);

    /// Wait for the restart predecessor before taking the instance lock. No-op for normal launches.
    static void waitForPredecessor();

    int restartOrExit(int exitCode) const;
    int restart(int exitCode) const;

private:
    QString m_workingDir;
};

#endif // RESTARTER_H
