#ifndef EDITTIMESIGNATUREDIALOG_H
#define EDITTIMESIGNATUREDIALOG_H

#include "UI/Dialogs/Base/OKCancelDialog.h"

class TimeSignatureEditWidget;

// Modal editor for one time signature point. The caller applies the values
// only when the dialog is accepted, so each confirmed edit is exactly one
// undoable operation.
class EditTimeSignatureDialog final : public OKCancelDialog {
    Q_OBJECT

public:
    explicit EditTimeSignatureDialog(int barIndex, int numerator, int denominator,
                                     QWidget *parent = nullptr);

    [[nodiscard]] int numerator() const;
    [[nodiscard]] int denominator() const;

private:
    TimeSignatureEditWidget *m_editWidget = nullptr;
};

#endif // EDITTIMESIGNATUREDIALOG_H
