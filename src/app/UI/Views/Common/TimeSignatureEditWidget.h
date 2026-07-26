#ifndef TIMESIGNATUREEDITWIDGET_H
#define TIMESIGNATUREEDITWIDGET_H

#include <QWidget>

class ComboBox;

namespace SVS {
    class ExpressionSpinBox;
}

// Numerator/denominator editors plus common-signature preset buttons, shared
// by the title bar popup (live apply) and the modal edit dialog (OK/Cancel).
// The denominator combo only offers powers of two, so invalid values cannot
// be produced from this widget.
class TimeSignatureEditWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimeSignatureEditWidget(QWidget *parent = nullptr);

    void setTimeSignature(int numerator, int denominator);
    [[nodiscard]] int numerator() const;
    [[nodiscard]] int denominator() const;

signals:
    void timeSignatureEdited(int numerator, int denominator);

private:
    void onPresetClicked(int numerator, int denominator);
    void setEditors(int numerator, int denominator);

    int m_numerator = 4;
    int m_denominator = 4;
    SVS::ExpressionSpinBox *m_spinNumerator = nullptr;
    ComboBox *m_cbDenominator = nullptr;
};

#endif // TIMESIGNATUREEDITWIDGET_H
