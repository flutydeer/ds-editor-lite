#ifndef RESTARTER_H
#define RESTARTER_H

#include <QString>

// Relaunches the application after the event loop exits if the "restart"
// property has been set on the application object.
class Restarter {
public:
    explicit Restarter(const QString &workingDir);

    int restartOrExit(int exitCode) const;
    int restart(int exitCode) const;

private:
    QString m_workingDir;
};

#endif // RESTARTER_H
