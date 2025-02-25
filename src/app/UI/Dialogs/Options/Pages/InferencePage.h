#ifndef INFERENCEPAGE_H
#define INFERENCEPAGE_H

#include "IOptionPage.h"

class LineEdit;
class ComboBox;
class SwitchButton;

namespace SVS {
    class ExpressionDoubleSpinBox;
    class SeekBar;
}

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
    SVS::SeekBar *m_dsDepthSlider;
    SVS::ExpressionDoubleSpinBox *m_dsDepthSpinBox;
    SwitchButton *m_swRunVocoderOnCpu;
    SwitchButton *m_autoStartInfer;
};



#endif // INFERENCEPAGE_H
