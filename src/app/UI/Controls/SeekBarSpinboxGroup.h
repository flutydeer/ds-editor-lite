#ifndef SEEKBARSPINBOXGROUP_H
#define SEEKBARSPINBOXGROUP_H

#include "UI/Controls/SvsSeekbar.h"
#include "UI/Controls/SvsExpressionspinbox.h"

class SeekBarSpinboxGroup final : public QObject {
    Q_OBJECT

public:
    SeekBarSpinboxGroup(const double min, const double max, const double step,
                        const double currentValue);

    void setValue(const double value) const;

    SVS::SeekBar *seekbar;
    SVS::ExpressionSpinBox *spinbox;
Q_SIGNALS:
    void valueChanged(double value);
    void editFinished();
};


#endif //SEEKBARSPINBOXGROUP_H