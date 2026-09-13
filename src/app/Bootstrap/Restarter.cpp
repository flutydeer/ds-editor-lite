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

    /// The environment variable through which a restarting process names itself to its
    /// successor. An environment variable rather than an argument, so that the successor starts
    /// with exactly the arguments the person gave, which is what a restart promises.
    constexpr char PREDECESSOR_VARIABLE[] = "DS_EDITOR_LITE_RESTART_PREDECESSOR";

}

Restarter::Restarter(const QString &workingDir) : m_workingDir(workingDir) {
}

void Restarter::waitForPredecessor() {
    const auto value = qEnvironmentVariable(PREDECESSOR_VARIABLE);
    if (value.isEmpty())
        return;
    // Cleared first, so that a restart of this process names this process and not the one that
    // started it.
    qunsetenv(PREDECESSOR_VARIABLE);
    bool ok = false;
    const auto pid = value.toLongLong(&ok);
    if (!ok || pid <= 0)
        return;
    // The predecessor is on its way out but may still hold the single-instance lock and the
    // ports this process is about to claim. Starting before it has let go would make this
    // process a secondary instance of a dying primary, which is the one thing a restart must not
    // become. Bounded, so a predecessor that does not exit cannot pin this one forever.
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
