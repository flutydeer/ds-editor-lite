#include "QuantizeDialog.h"

#include "UI/Utils/QuantizeOptions.h"
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

QuantizeDialog::QuantizeDialog(QWidget *parent) : OKCancelDialog(parent) {
    setWindowTitle(tr("Quantize"));
    setTitle(tr("Quantize"));

    m_cbQuantize = new ComboBox(true);
    m_cbQuantize->addItems(QuantizeOptions::strings());

    m_chkStart = new QCheckBox(tr("Quantize start position"));
    m_chkStart->setChecked(true);
    m_chkLength = new QCheckBox(tr("Quantize length"));
    m_chkLength->setChecked(true);

    const auto formLayout = new QFormLayout;
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->addRow(tr("Grid:"), m_cbQuantize);
    formLayout->addRow(m_chkStart);
    formLayout->addRow(m_chkLength);

    const auto bodyLayout = new QVBoxLayout(body());
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->addLayout(formLayout);

    connect(okButton(), &AccentButton::clicked, this, &QDialog::accept);
    connect(cancelButton(), &Button::clicked, this, &QDialog::reject);
}

void QuantizeDialog::setQuantize(int quantize) {
    m_cbQuantize->setCurrentIndex(QuantizeOptions::indexOf(quantize));
}

int QuantizeDialog::quantize() const {
    return QuantizeOptions::values().at(m_cbQuantize->currentIndex());
}

bool QuantizeDialog::quantizeStart() const {
    return m_chkStart->isChecked();
}

bool QuantizeDialog::quantizeLength() const {
    return m_chkLength->isChecked();
}
