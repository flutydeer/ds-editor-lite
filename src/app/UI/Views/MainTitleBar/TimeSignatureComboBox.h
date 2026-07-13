//
// Created by FlutyDeer on 2026/7/13.
//

#ifndef TIMESIGNATURECOMBOBOX_H
#define TIMESIGNATURECOMBOBOX_H

#include "UI/Controls/InlineEditLabel.h"

class TimeSignaturePopupWidget;
class QMouseEvent;
class QTimer;

class TimeSignatureComboBox : public InlineEditLabel {
    Q_OBJECT

public:
    explicit TimeSignatureComboBox(QWidget *parent = nullptr);
    ~TimeSignatureComboBox() override = default;

    void setTimeSignature(int numerator, int denominator);

signals:
    void timeSignatureChanged(int numerator, int denominator);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showPopup();
    void hidePopup();
    void setPopupVisible(bool visible);

    TimeSignaturePopupWidget *m_popup = nullptr;
    QTimer *m_clickTimer = nullptr;
    int m_numerator = 4;
    int m_denominator = 4;
    bool m_ignoreNextShow = false;
    bool m_hidingPopupFromCombo = false;
};

#endif // TIMESIGNATURECOMBOBOX_H
