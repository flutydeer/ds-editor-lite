#include "EditTempoDialog.h"

#include "UI/Views/Common/TempoEditWidget.h"
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>

#include <QVBoxLayout>

EditTempoDialog::EditTempoDialog(const int tick, const double tempo, QWidget *parent)
    : OKCancelDialog(parent) {
    setWindowTitle(tr("Tempo"));
    setTitle(tr("Tempo"));
    setMessage(tr("Tick %L1").arg(tick));

    m_editWidget = new TempoEditWidget;
    m_editWidget->setTempo(tempo);

    const auto bodyLayout = new QVBoxLayout(body());
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->addWidget(m_editWidget);

    connect(okButton(), &AccentButton::clicked, this, &QDialog::accept);
    connect(cancelButton(), &Button::clicked, this, &QDialog::reject);
}

double EditTempoDialog::tempo() const {
    return m_editWidget->tempo();
}
