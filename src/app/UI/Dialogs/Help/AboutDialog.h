#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

class QLabel;

// The "About" box: product identity, build metadata (from <lite/BuildInfo.h>)
// and license. Built on the shared Dialog base (frameless + theme-aware).
class AboutDialog final : public Dialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);

private:
    [[nodiscard]] QString buildHtml() const;

    QLabel *m_textLabel = nullptr;
    QLabel *m_iconLabel = nullptr;
};

#endif // ABOUTDIALOG_H
