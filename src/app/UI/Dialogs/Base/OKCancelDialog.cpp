//
// Created by fluty on 24-3-17.
//

#include "OKCancelDialog.h"

#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"

OKCancelDialog::OKCancelDialog(QWidget *parent) : Dialog(parent) {
    m_btnOk = new AccentButton(tr("&OK"), this);
    m_btnCancel = new Button(tr("&Cancel"), this);

    setPositiveButton(m_btnOk);
    setNegativeButton(m_btnCancel);
}
AccentButton *OKCancelDialog::okButton() const {
    return m_btnOk;
}
Button *OKCancelDialog::cancelButton() const {
    return m_btnCancel;
}