//
// Created by fluty on 24-8-12.
//

#ifndef RESTARTDIALOG_H
#define RESTARTDIALOG_H

#include "Dialog.h"

class AccentButton;
class RestartDialog : public Dialog {
    Q_OBJECT

public:
    explicit RestartDialog(const QString &message, bool canRestartLater = true,
                           QWidget *parent = nullptr);

private:
    bool m_canRestartLater;
    AccentButton *m_btnRestartNow = nullptr;
    Button *m_btnRestartLater = nullptr;
};



#endif // RESTARTDIALOG_H
