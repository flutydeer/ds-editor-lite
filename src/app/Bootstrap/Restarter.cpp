#include "Restarter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QtCore/QProcess>
#include <QVariant>

Restarter::Restarter(const QString &workingDir) : m_workingDir(workingDir) {
}

int Restarter::restartOrExit(int exitCode) const {
    const auto *application = QCoreApplication::instance();
    return application && application->property("restart").toBool() ? restart(exitCode) : exitCode;
}

int Restarter::restart(int exitCode) const {
    const auto applicationFilePath = QCoreApplication::applicationFilePath();
    const auto applicationArguments = QCoreApplication::arguments().mid(1);
    const auto started =
        QProcess::startDetached(applicationFilePath, applicationArguments, m_workingDir);
    if (started)
        qDebug() << "Restarting application..." << applicationFilePath << applicationArguments;
    else
        qWarning() << "Failed to restart application" << applicationFilePath;
    return exitCode;
}
