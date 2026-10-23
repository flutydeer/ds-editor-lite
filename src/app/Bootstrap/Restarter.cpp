#include "Restarter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QProcessEnvironment>
#include <QThread>
#include <QVariant>
#include <QtCore/QProcess>

#include "ProcessProbe.h"

namespace {

    /// Pass the predecessor PID without changing the user's command-line arguments.
    constexpr char PREDECESSOR_VARIABLE[] = "DS_EDITOR_LITE_RESTART_PREDECESSOR";

}

Restarter::Restarter(const QString &workingDir) : m_workingDir(workingDir) {
}

void Restarter::waitForPredecessor() {
    const auto value = qEnvironmentVariable(PREDECESSOR_VARIABLE);
    if (value.isEmpty())
        return;
    // Do not pass the predecessor's PID to a subsequent restart.
    qunsetenv(PREDECESSOR_VARIABLE);
    bool ok = false;
    const auto pid = value.toLongLong(&ok);
    if (!ok || pid <= 0)
        return;
    // Wait for the predecessor to release its lock and ports before claiming them.
    QElapsedTimer timer;
    timer.start();
    while (ProcessProbe::isAlive(pid) && timer.elapsed() < 30000)
        QThread::msleep(20);
    if (ProcessProbe::isAlive(pid))
        qWarning() << "The process being restarted did not exit in time:" << pid;
}

int Restarter::restartOrExit(int exitCode) const {
    const auto *application = QCoreApplication::instance();
    return application && application->property("restart").toBool() ? restart(exitCode) : exitCode;
}

int Restarter::restart(int exitCode) const {
    QProcess successor;
    successor.setProgram(QCoreApplication::applicationFilePath());
    successor.setArguments(QCoreApplication::arguments().mid(1));
    successor.setWorkingDirectory(m_workingDir);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QString::fromLatin1(PREDECESSOR_VARIABLE),
                       QString::number(QCoreApplication::applicationPid()));
    successor.setProcessEnvironment(environment);
    const auto started = successor.startDetached();
    if (started)
        qDebug() << "Restarting application..." << successor.program() << successor.arguments();
    else
        qWarning() << "Failed to restart application" << successor.program();
    return exitCode;
}
