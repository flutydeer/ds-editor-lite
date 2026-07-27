#ifndef EDITTEMPODIALOG_H
#define EDITTEMPODIALOG_H

#include "UI/Dialogs/Base/OKCancelDialog.h"

class TempoEditWidget;

// Modal editor for one tempo-map point. The caller commits only on accept, so
// an insert or edit is represented by exactly one undoable history entry.
class EditTempoDialog final : public OKCancelDialog {
    Q_OBJECT

public:
    explicit EditTempoDialog(int tick, double tempo, QWidget *parent = nullptr);

    [[nodiscard]] double tempo() const;

private:
    TempoEditWidget *m_editWidget = nullptr;
};

#endif // EDITTEMPODIALOG_H
