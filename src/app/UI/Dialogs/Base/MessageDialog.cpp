#include "MessageDialog.h"
#include <QLabel>
#include <QPushButton>

MessageDialog::MessageDialog(const QString &title, const QString &message, QWidget *parent)
    : Dialog(parent), clickedButtonId(-1) {
    setWindowTitle(title);
    setModal(true);

    mainlayout = new QVBoxLayout();
    m_buttonlayout = new QHBoxLayout();
    m_buttonlayout->addStretch();

    messageLabel = new QLabel(message, this);
    mainlayout->addWidget(messageLabel);
    mainlayout->addLayout(m_buttonlayout);
    this->body()->setLayout(mainlayout);
}

QVBoxLayout *MessageDialog::mainLayout() const {
    return mainlayout;
}

void MessageDialog::addButton(const QString &text, int buttonId) {
    auto *button = new Button(text, this);
    m_buttonlayout->addWidget(button);

    buttons[buttonId] = button;

    connect(button, &QPushButton::clicked, this,
            [this, buttonId]() { handleButtonClicked(buttonId); });
}

void MessageDialog::addAccentButton(const QString &text, int buttonId) {
    auto *button = new AccentButton(text, this);
    m_buttonlayout->addWidget(button);

    buttons[buttonId] = button;

    connect(button, &QPushButton::clicked, this,
            [this, buttonId]() { handleButtonClicked(buttonId); });
}

void MessageDialog::handleButtonClicked(int buttonId) {
    clickedButtonId = buttonId;
    accept();
}

int MessageDialog::exec() {
    QDialog::exec();
    return clickedButtonId;
}
