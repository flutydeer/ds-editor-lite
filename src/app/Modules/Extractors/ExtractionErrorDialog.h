#ifndef EXTRACTIONERRORDIALOG_H
#define EXTRACTIONERRORDIALOG_H

#include <QString>

namespace Extractors {

    /// Shows a modal dialog that reports a failed extraction.
    ///
    /// The title and the button text are passed translated, so that each controller keeps its own
    /// translation context.
    void showErrorDialog(const QString &title, const QString &closeText, const QString &message);

}

#endif // EXTRACTIONERRORDIALOG_H
