//
// Created by fluty on 24-3-17.
//

#ifndef OKCANCELAPPLYDIALOG_H
#define OKCANCELAPPLYDIALOG_H

#include "Dialog.h"

class AccentButton;

class OKCancelDialog : public Dialog {
    Q_OBJECT

public:
    explicit OKCancelDialog(QWidget *parent = nullptr);

    [[nodiscard]] AccentButton *okButton() const;
    [[nodiscard]] Button *cancelButton() const;

private:
    AccentButton *m_btnOk;
    Button *m_btnCancel;
};



#endif // OKCANCELAPPLYDIALOG_H
