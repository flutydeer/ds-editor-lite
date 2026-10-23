#ifndef EXTRACTIONERRORDIALOG_H
#define EXTRACTIONERRORDIALOG_H

#include <QString>

namespace Extractors {

    /// Dialog title and button text are translated by the caller.
    void showErrorDialog(const QString &title, const QString &closeText, const QString &message);

}

#endif // EXTRACTIONERRORDIALOG_H
