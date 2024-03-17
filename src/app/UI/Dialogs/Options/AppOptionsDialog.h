//
// Created by fluty on 24-3-16.
//

#ifndef APPOPTIONSDIALOG_H
#define APPOPTIONSDIALOG_H

#include "UI/Dialogs/Base/OKCancelApplyDialog.h"

class Button;

class AppOptionsDialog : public OKCancelApplyDialog {
    Q_OBJECT

public:
    explicit AppOptionsDialog(QWidget *parent = nullptr);

private slots:
    void apply();
    void cancel();
};



#endif // APPOPTIONSDIALOG_H
