//
// Created by fluty on 24-8-12.
//

#include "RestartDialog.h"

#include "Controller/AppController.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"

#include <QApplication>

RestartDialog::RestartDialog(const QString &message, bool canRestartLater, QWidget *parent)
    : Dialog(parent), m_canRestartLater(canRestartLater) {
    setModal(true);
    setMinimumWidth(360);
    setWindowTitle(tr("Information"));
    setTitle(tr("%1 requires a restart").arg(QApplication::applicationDisplayName()));
    setMessage(message);

    m_btnRestartNow = new AccentButton(tr("Restart Now"));
    setPositiveButton(m_btnRestartNow);
    connect(m_btnRestartNow, &Button::clicked, this, &Dialog::accept);
    connect(this, &Dialog::accepted, appController, &AppController::restart);

    if (m_canRestartLater) {
        m_btnRestartLater = new Button(tr("Restart Later"));
        setNegativeButton(m_btnRestartLater);
        connect(m_btnRestartLater, &Button::clicked, this, &Dialog::reject);
    }
}