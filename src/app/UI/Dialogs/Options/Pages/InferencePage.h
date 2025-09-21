#ifndef INFERENCEPAGE_H
#define INFERENCEPAGE_H

#include "IOptionPage.h"


class LineEdit;
class ComboBox;
class SwitchButton;
class SeekBarSpinboxGroup;
class DoubleSeekBarSpinboxGroup;
class QTreeView;

class InferencePage : public IOptionPage {
    Q_OBJECT

public:
    explicit InferencePage(QWidget *parent = nullptr);
    // ~InferencePage() override;

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    ComboBox *m_cbExecutionProvider;
    ComboBox *m_cbDeviceList;
    ComboBox *m_cbSamplingSteps;
    DoubleSeekBarSpinboxGroup *m_dsDepthSlider;
    SwitchButton *m_swRunVocoderOnCpu;
    SwitchButton *m_autoStartInfer;
    SeekBarSpinboxGroup *m_smoothSlider;
    QTreeView *m_treeView;
};


#endif // INFERENCEPAGE_H