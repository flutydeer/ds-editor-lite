#include "MessageDialog.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MessageDialog::MessageDialog(const QString &title, const QString &message, QWidget *parent)
    : Dialog(parent) {
    setWindowTitle(title);
    setModal(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // The Dialog base does not install a layout on body(); icon rows and the
    // optional checkbox land here. The body itself already carries 12px margins.
    const auto bodyLayout = new QVBoxLayout(body());
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(12);

    setMessage(message);
}

QCheckBox *MessageDialog::addCheckBox(const QString &text) {
    if (m_checkBox)
        return nullptr;
    m_checkBox = new QCheckBox(text, this);
    if (const auto bodyLayout = qobject_cast<QVBoxLayout *>(body()->layout()))
        bodyLayout->addWidget(m_checkBox);
    return m_checkBox;
}

bool MessageDialog::isCheckBoxChecked() const {
    return m_checkBox && m_checkBox->isChecked();
}

void MessageDialog::addButton(const QString &text, int buttonId) {
    auto *button = new Button(text, this);
    buttonBar()->addButton(button);

    buttons[buttonId] = button;

    connect(button, &QPushButton::clicked, this,
            [this, buttonId] { handleButtonClicked(buttonId); });
}

void MessageDialog::addAccentButton(const QString &text, int buttonId) {
    auto *button = new AccentButton(text, this);
    buttonBar()->addButton(button);

    buttons[buttonId] = button;

    connect(button, &QPushButton::clicked, this,
            [this, buttonId] { handleButtonClicked(buttonId); });
}

void MessageDialog::handleButtonClicked(const int buttonId) {
    clickedButtonId = buttonId;
    accept();
}

int MessageDialog::exec() {
    QDialog::exec();
    return clickedButtonId;
}
