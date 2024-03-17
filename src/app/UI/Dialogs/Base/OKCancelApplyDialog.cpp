//
// Created by fluty on 24-3-17.
//

#include "OKCancelApplyDialog.h"

#include "UI/Controls/Button.h"

OKCancelApplyDialog::OKCancelApplyDialog(QWidget *parent) : Dialog(parent) {
    m_btnOk = new Button(tr("&OK"));
    m_btnOk->setPrimary(true);
    m_btnCancel = new Button(tr("&Cancel"));
    m_btnApply = new Button(tr("&Apply"));

    setPositiveButton(m_btnOk);
    setNegativeButton(m_btnCancel);
    setNeutralButton(m_btnApply);
}
OKCancelApplyDialog::~OKCancelApplyDialog() {
    delete m_btnOk;
    delete m_btnCancel;
    delete m_btnApply;
}
Button *OKCancelApplyDialog::okButton() {
    return m_btnOk;
}
Button *OKCancelApplyDialog::cancelButton() {
    return m_btnCancel;
}
Button *OKCancelApplyDialog::applyButton() {
    return m_btnApply;
}