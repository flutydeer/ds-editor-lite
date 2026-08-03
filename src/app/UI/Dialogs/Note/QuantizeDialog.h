#ifndef QUANTIZEDIALOG_H
#define QUANTIZEDIALOG_H

#include "UI/Dialogs/Base/OKCancelDialog.h"

class ComboBox;
class QCheckBox;

class QuantizeDialog : public OKCancelDialog {
    Q_OBJECT

public:
    explicit QuantizeDialog(QWidget *parent = nullptr);

    void setQuantize(int quantize);
    [[nodiscard]] int quantize() const;
    [[nodiscard]] bool quantizeStart() const;
    [[nodiscard]] bool quantizeLength() const;

private:
    ComboBox *m_cbQuantize = nullptr;
    QCheckBox *m_chkStart = nullptr;
    QCheckBox *m_chkLength = nullptr;
};

#endif // QUANTIZEDIALOG_H
