#include "Restarter.h"

#include <QApplication>
#include <QDebug>
#include <QtCore/QProcess>

Restarter::Restarter(const QString &workingDir) : m_workingDir(workingDir) {
}

int Restarter::restartOrExit(int exitCode) const {
    return qApp->property("restart").toBool() ? restart(exitCode) : exitCode;
}

int Restarter::restart(int exitCode) const {
    const auto applicationFilePath = QApplication::applicationFilePath();
    const auto applicationArguments = QApplication::arguments().mid(1);
    const auto started =
        QProcess::startDetached(applicationFilePath, applicationArguments, m_workingDir);
    if (started)
        qDebug() << "Restarting application..." << applicationFilePath << applicationArguments;
    else
        qWarning() << "Failed to restart application" << applicationFilePath;
    return exitCode;
}
