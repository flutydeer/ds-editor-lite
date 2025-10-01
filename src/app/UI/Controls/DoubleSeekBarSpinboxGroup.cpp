#include "DoubleSeekBarSpinboxGroup.h"

DoubleSeekBarSpinboxGroup::DoubleSeekBarSpinboxGroup(const double min, const double max,
                                                     const double step, const double currentValue)
    : seekbar(new SVS::SeekBar()), spinbox(new SVS::ExpressionDoubleSpinBox()) {
    seekbar->setRange(min, max);
    seekbar->setSingleStep(step);
    seekbar->setValue(currentValue);

    spinbox->setRange(min, max);
    spinbox->setSingleStep(step);
    spinbox->setValue(currentValue);


    connect(seekbar, &SVS::SeekBar::valueChanged, this, [&](const double value) {
        spinbox->setValue(value);
        Q_EMIT valueChanged(value);
    });
    connect(seekbar, &SVS::SeekBar::sliderReleased, this, [&] { Q_EMIT editFinished(); });

    connect(spinbox, &SVS::ExpressionDoubleSpinBox::valueChanged, this, [&](const double value) {
        seekbar->setValue(value);
        Q_EMIT valueChanged(value);
    });
    connect(spinbox, &SVS::ExpressionDoubleSpinBox::editingFinished, this,
            [&] { Q_EMIT editFinished(); });
}

void DoubleSeekBarSpinboxGroup::setValue(const double value) const {
    seekbar->setValue(value);
    spinbox->setValue(value);
}