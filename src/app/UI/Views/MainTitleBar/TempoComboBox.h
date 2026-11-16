#ifndef TEMPOCOMBOBOX_H
#define TEMPOCOMBOBOX_H

#include "UI/Views/Common/PopupInlineEditLabel.h"

class TempoPopupWidget;

class TempoComboBox : public PopupInlineEditLabel {
    Q_OBJECT

public:
    explicit TempoComboBox(QWidget *parent = nullptr);
    ~TempoComboBox() override = default;

    void setTempo(double tempo);

signals:
    void tempoChanged(double tempo);

protected:
    void preparePopup() override;
    void presentPopup(const QPoint &globalPos) override;

private:
    TempoPopupWidget *m_popup = nullptr;
    double m_tempo = 120.0;
};

#endif // TEMPOCOMBOBOX_H
