#ifndef TEMPOCOMBOBOX_H
#define TEMPOCOMBOBOX_H

#include "UI/Controls/InlineEditLabel.h"

class TempoPopupWidget;
class QMouseEvent;
class QTimer;

class TempoComboBox : public InlineEditLabel {
    Q_OBJECT

public:
    explicit TempoComboBox(QWidget *parent = nullptr);
    ~TempoComboBox() override = default;

    void setTempo(double tempo);

signals:
    void tempoChanged(double tempo);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showPopup();
    void hidePopup();
    void setPopupVisible(bool visible);

    TempoPopupWidget *m_popup = nullptr;
    QTimer *m_clickTimer = nullptr;
    double m_tempo = 120.0;
    bool m_ignoreNextShow = false;
    bool m_hidingPopupFromCombo = false;
};

#endif // TEMPOCOMBOBOX_H
