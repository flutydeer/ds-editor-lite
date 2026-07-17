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
    qDebug() << "Restarting application...";
    QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments(),
                            m_workingDir);
    return exitCode;
}
