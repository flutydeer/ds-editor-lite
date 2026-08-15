#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QMessageBox>

// The "About" box: product identity, build metadata (from <lite/BuildInfo.h>)
// and license. Construct and exec() it like any dialog.
class AboutDialog final : public QMessageBox {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

#endif // ABOUTDIALOG_H
