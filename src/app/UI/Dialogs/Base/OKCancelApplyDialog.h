//
// Created by fluty on 24-3-17.
//

#ifndef OKCANCELAPPLYDIALOG_H
#define OKCANCELAPPLYDIALOG_H

#include "Dialog.h"

class OKCancelApplyDialog : public Dialog {
    Q_OBJECT

public:
    explicit OKCancelApplyDialog(QWidget *parent = nullptr);
    ~OKCancelApplyDialog() override;

    Button *okButton();
    Button *cancelButton();
    Button *applyButton();

private:
    Button *m_btnOk;
    Button *m_btnCancel;
    Button *m_btnApply;
};



#endif //OKCANCELAPPLYDIALOG_H
