#include "EditTimeSignatureDialog.h"

#include "UI/Views/Common/TimeSignatureEditWidget.h"
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>

#include <QVBoxLayout>

EditTimeSignatureDialog::EditTimeSignatureDialog(const int barIndex, const int numerator,
                                                 const int denominator, QWidget *parent)
    : OKCancelDialog(parent) {
    setWindowTitle(tr("Time Signature"));
    setTitle(tr("Time Signature"));
    // Bars are 0-based in the model but displayed 1-based
    setMessage(tr("Bar %L1").arg(barIndex + 1));

    m_editWidget = new TimeSignatureEditWidget;
    m_editWidget->setTimeSignature(numerator, denominator);

    const auto bodyLayout = new QVBoxLayout(body());
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->addWidget(m_editWidget);

    connect(okButton(), &AccentButton::clicked, this, &QDialog::accept);
    connect(cancelButton(), &Button::clicked, this, &QDialog::reject);
}

int EditTimeSignatureDialog::numerator() const {
    return m_editWidget->numerator();
}

int EditTimeSignatureDialog::denominator() const {
    return m_editWidget->denominator();
}
