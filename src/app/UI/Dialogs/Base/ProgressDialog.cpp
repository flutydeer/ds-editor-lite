#include "ProgressDialog.h"

#include "UI/Controls/Button.h"
#include "UI/Controls/ProgressIndicator.h"

#include <QCloseEvent>
#include <QHBoxLayout>

ProgressDialog::ProgressDialog(const bool cancellable, const bool canHide, QWidget *parent)
    : Dialog(parent), m_canHide(canHide), m_cancellable(cancellable) {
    setModal(true);
    setMinimumWidth(360);

    m_progressBar = new ProgressIndicator(this);
    m_progressBar->setIndeterminate(true);

    if (cancellable) {
        m_btnCancel = new Button(tr("Cancel"), this);
        setNegativeButton(m_btnCancel);
        connect(m_btnCancel, &Button::clicked, this, [this] { onCanceled(); });
    }
    if (canHide) {
        m_btnHide = new Button(tr("Hide"), this);
        setPositiveButton(m_btnHide);
        connect(m_btnHide, &Button::clicked, this, &ProgressDialog::accept);
    } else if (!cancellable) {
        setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    }

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(m_progressBar);
    mainLayout->setContentsMargins({});
    body()->setLayout(mainLayout);
}

void ProgressDialog::forceClose() {
    m_canHide = true;
    accept();
}

void ProgressDialog::setProgressRange(const double minimum, const double maximum) const {
    m_progressBar->setMinimum(minimum);
    m_progressBar->setMaximum(maximum);
}

void ProgressDialog::setProgressValue(const double value) const {
    m_progressBar->setValue(value);
}

void ProgressDialog::setProgressIndeterminate(const bool indeterminate) const {
    m_progressBar->setIndeterminate(indeterminate);
}

void ProgressDialog::setProgressStatus(const TaskGlobal::Status status) const {
    m_progressBar->setTaskStatus(status);
}

void ProgressDialog::setCancellationEnabled(const bool enabled) {
    m_cancellable = enabled;
    if (m_btnCancel)
        m_btnCancel->setEnabled(enabled);
}

void ProgressDialog::closeEvent(QCloseEvent *event) {
    if (m_canHide) {
        Dialog::closeEvent(event);
    } else if (m_cancellable) {
        onCanceled();
        event->ignore();
    } else {
        event->ignore();
    }
}

void ProgressDialog::onCanceled() {
    emit canceled();
}
