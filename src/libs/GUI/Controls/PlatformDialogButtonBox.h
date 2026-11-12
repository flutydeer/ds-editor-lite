#ifndef PLATFORMDIALOGBUTTONBOX_H
#define PLATFORMDIALOGBUTTONBOX_H

#include <QDialogButtonBox>

// Keep the application's button styling, but use a platform-specific role layout.
// This class deliberately does not connect accepted/rejected to a dialog: some
// actions cancel a task or open a folder without closing their window.
class PlatformDialogButtonBox : public QDialogButtonBox {
    Q_OBJECT

public:
    explicit PlatformDialogButtonBox(QWidget *parent = nullptr);
    explicit PlatformDialogButtonBox(ButtonLayout layout, QWidget *parent = nullptr);
    [[nodiscard]] static ButtonLayout platformLayout();
};

#endif // PLATFORMDIALOGBUTTONBOX_H
