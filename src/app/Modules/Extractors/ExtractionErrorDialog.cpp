#include "ExtractionErrorDialog.h"

#include "UI/Dialogs/Base/Dialog.h"

#include <lite/GUI/Controls/AccentButton.h>

namespace Extractors {

    void showErrorDialog(const QString &title, const QString &closeText, const QString &message) {
        Dialog dialog;
        dialog.setTitle(title);
        dialog.setMessage(message);
        dialog.setModal(true);
        const auto close = new AccentButton(closeText);
        QObject::connect(close, &Button::clicked, &dialog, &Dialog::accept);
        dialog.setPositiveButton(close);
        dialog.exec();
    }

}
