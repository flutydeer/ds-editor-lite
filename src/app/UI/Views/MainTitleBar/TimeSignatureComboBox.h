#ifndef TIMESIGNATURECOMBOBOX_H
#define TIMESIGNATURECOMBOBOX_H

#include "UI/Views/Common/PopupInlineEditLabel.h"

class TimeSignaturePopupWidget;

class TimeSignatureComboBox : public PopupInlineEditLabel {
    Q_OBJECT

public:
    explicit TimeSignatureComboBox(QWidget *parent = nullptr);
    ~TimeSignatureComboBox() override = default;

    void setTimeSignature(int numerator, int denominator);

signals:
    void timeSignatureChanged(int numerator, int denominator);

protected:
    void preparePopup() override;
    void presentPopup(const QPoint &globalPos) override;

private:
    TimeSignaturePopupWidget *m_popup = nullptr;
    int m_numerator = 4;
    int m_denominator = 4;
};

#endif // TIMESIGNATURECOMBOBOX_H
