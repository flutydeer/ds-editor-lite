#include "DsspController.h"

#include "DsspServer.h"
#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logDsspController, "dssp.controller")

DsspController::DsspController(QObject *parent) : QObject(parent) {
    m_server = new DsspServer(this);
    connect(appOptions, &AppOptions::optionsChanged, this,
            [this](AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::Dssp)
                    applySettings();
            });
    applySettings();
}

DsspController::~DsspController() {
    // Stop the server synchronously: joins the listener thread and the worker
    // pool, letting in-flight syntheses finish against the still-alive
    // SynthrtEngine.
    stopService();
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(DsspController)

bool DsspController::isServiceRunning() const {
    return m_server && m_server->isRunning();
}

void DsspController::applySettings() {
    const auto option = appOptions->dssp();
    if (option->enabled)
        startService();
    else
        stopService();
}

void DsspController::startService() {
    const auto option = appOptions->dssp();
    if (m_server->isRunning()) {
        // Address unchanged: keep the current listener.
        if (m_lastHost == option->host && m_lastPort == option->port)
            return;
        m_server->stop();
    }
    QString errorMessage;
    if (m_server->start(option->host, option->port, &errorMessage)) {
        m_lastHost = option->host;
        m_lastPort = option->port;
    } else {
        qCWarning(logDsspController).noquote()
            << "Failed to start DSSP service:" << errorMessage;
    }
}

void DsspController::stopService() {
    m_server->stop();
}
