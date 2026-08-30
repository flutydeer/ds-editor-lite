#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include <QVBoxLayout>
#include <QLabel>

#include "Dialog.h"
#include <lite/GUI/Controls/AccentButton.h>
#include <QMap>

class QCheckBox;

class MessageDialog : public Dialog {
    Q_OBJECT

public:
    explicit MessageDialog(const QString &title = "", const QString &message = "",
                           QWidget *parent = nullptr);

    void addButton(const QString &text, int buttonId);
    void addAccentButton(const QString &text, int buttonId);

    // Adds an unchecked checkbox below the message. Returns nullptr if a checkbox
    // was already added. Read the state back via isCheckBoxChecked() after exec().
    QCheckBox *addCheckBox(const QString &text);

    [[nodiscard]] bool isCheckBoxChecked() const;

    int exec() override;

private slots:
    void handleButtonClicked(int buttonId);

private:
    int clickedButtonId = -1;
    QMap<int, Button *> buttons;
    QCheckBox *m_checkBox = nullptr;
};

#endif // MESSAGEDIALOG_H
