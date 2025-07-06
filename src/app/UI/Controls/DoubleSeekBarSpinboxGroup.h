#ifndef DOUBLESEEKBARSPINBOXGROUP_H
#define DOUBLESEEKBARSPINBOXGROUP_H


#include "UI/Controls/SvsSeekbar.h"
#include "UI/Controls/SvsExpressionDoubleSpinBox.h"

class DoubleSeekBarSpinboxGroup final : public QObject {
    Q_OBJECT

public:
    DoubleSeekBarSpinboxGroup(const double min, const double max, const double step,
                              const double currentValue);

    void setValue(const double value) const;

    SVS::SeekBar *seekbar;
    SVS::ExpressionDoubleSpinBox *spinbox;
Q_SIGNALS:
    void valueChanged(double value);
    void editFinished();
};


#endif //DOUBLESEEKBARSPINBOXGROUP_H