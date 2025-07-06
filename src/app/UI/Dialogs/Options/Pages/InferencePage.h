#ifndef INFERENCEPAGE_H
#define INFERENCEPAGE_H

#include "IOptionPage.h"

#include "UI/Controls/SeekBarSpinboxGroup.h"
#include "UI/Controls/DoubleSeekBarSpinboxGroup.h"


class LineEdit;
class ComboBox;
class SwitchButton;

class InferencePage : public IOptionPage {
    Q_OBJECT

public:
    explicit InferencePage(QWidget *parent = nullptr);
    // ~InferencePage() override;

protected:
    void modifyOption() override;

private:
    ComboBox *m_cbExecutionProvider;
    ComboBox *m_cbDeviceList;
    ComboBox *m_cbSamplingSteps;
    DoubleSeekBarSpinboxGroup *m_dsDepthSlider;
    SVS::ExpressionDoubleSpinBox *m_dsDepthSpinBox;
    SwitchButton *m_swRunVocoderOnCpu;
    SwitchButton *m_autoStartInfer;
    SeekBarSpinboxGroup *m_smoothSlider;
};


#endif // INFERENCEPAGE_H