//
// Created by FlutyDeer on 2026/7/13.
//

#ifndef TIMESIGNATUREPOPUPWIDGET_H
#define TIMESIGNATUREPOPUPWIDGET_H

#include <QFrame>

class ComboBox;
class QLabel;

namespace SVS {
    class ExpressionSpinBox;
}

class TimeSignaturePopupWidget : public QFrame {
    Q_OBJECT

public:
    explicit TimeSignaturePopupWidget(QWidget *parent = nullptr);

    void setTimeSignature(int numerator, int denominator);
    void showAt(const QPoint &globalPos);

signals:
    void timeSignatureSelected(int numerator, int denominator);

protected:
    void changeEvent(QEvent *event) override;

private:
    void applyWindowEffects();
    void onPresetClicked(int numerator, int denominator);
    void setEditors(int numerator, int denominator);

    int m_numerator = 4;
    int m_denominator = 4;
    SVS::ExpressionSpinBox *m_spinNumerator = nullptr;
    ComboBox *m_cbDenominator = nullptr;
    QLabel *m_titleLabel = nullptr;
};

#endif // TIMESIGNATUREPOPUPWIDGET_H
