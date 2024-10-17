#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include <QVBoxLayout>
#include <QMap>
#include <QLabel>

#include "Dialog.h"
#include "UI/Controls/AccentButton.h"

class MessageDialog : public Dialog {
    Q_OBJECT

public:
    explicit MessageDialog(const QString &title = "", const QString &message = "",
                           QWidget *parent = nullptr);

    QVBoxLayout *mainLayout() const;

    void addButton(const QString &text, int buttonId);

    int exec() override;

private slots:
    void handleButtonClicked(int buttonId);

private:
    QVBoxLayout *mainlayout;
    QHBoxLayout *m_buttonlayout;
    QLabel *messageLabel;
    int clickedButtonId = -1;
    QMap<int, AccentButton *> buttons;
};

#endif // MESSAGEDIALOG_H
