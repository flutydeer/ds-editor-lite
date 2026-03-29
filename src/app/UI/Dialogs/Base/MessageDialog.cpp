#include "MessageDialog.h"
#include <QLabel>
#include <QPushButton>

MessageDialog::MessageDialog(const QString &title, const QString &message, QWidget *parent)
    : Dialog(parent) {
    setWindowTitle(title);
    setModal(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // mainlayout = new QVBoxLayout();

    setMessage(message);
    // messageLabel = new QLabel(message, this);
    // mainlayout->addWidget(messageLabel);
    // this->body()->setLayout(mainlayout);
}

// QVBoxLayout *MessageDialog::mainLayout() const {
//     return mainlayout;
// }

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
