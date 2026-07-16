#ifndef MIDIPAGE_H
#define MIDIPAGE_H

#include "IOptionPage.h"

class ComboBox;
class OptionListCard;
class OptionsCardItem;
class QPushButton;
class SettingPageSynthHelper;
class SwitchButton;

namespace SVS {
    class SeekBar;
    class ExpressionDoubleSpinBox;
    class ExpressionSpinBox;
}

class MidiPage : public IOptionPage {
    Q_OBJECT
public:
    explicit MidiPage(QWidget *parent = nullptr);
    ~MidiPage() override;

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;
    void changeEvent(QEvent *event) override;

private:
    void retranslateUi();

    SettingPageSynthHelper *m_synthHelper = nullptr;
    ComboBox *m_deviceComboBox = nullptr;
    ComboBox *m_generatorComboBox = nullptr;
    SVS::SeekBar *m_amplitudeSlider = nullptr;
    SVS::ExpressionDoubleSpinBox *m_amplitudeSpinBox = nullptr;
    SVS::SeekBar *m_attackSlider = nullptr;
    SVS::ExpressionSpinBox *m_attackSpinBox = nullptr;
    SVS::SeekBar *m_decaySlider = nullptr;
    SVS::ExpressionSpinBox *m_decaySpinBox = nullptr;
    SVS::SeekBar *m_decayRatioSlider = nullptr;
    SVS::ExpressionDoubleSpinBox *m_decayRatioSpinBox = nullptr;
    SVS::SeekBar *m_releaseSlider = nullptr;
    SVS::ExpressionSpinBox *m_releaseSpinBox = nullptr;
    SVS::ExpressionDoubleSpinBox *m_frequencyOfASpinBox = nullptr;
    SwitchButton *m_adjustByProjectSwitch = nullptr;
    QPushButton *m_synthesizerTestButton = nullptr;
    QPushButton *m_flushButton = nullptr;
    OptionListCard *m_inputCard = nullptr;
    OptionListCard *m_synthesizerCard = nullptr;
    OptionsCardItem *m_deviceItem = nullptr;
    OptionsCardItem *m_generatorItem = nullptr;
    OptionsCardItem *m_amplitudeItem = nullptr;
    OptionsCardItem *m_attackItem = nullptr;
    OptionsCardItem *m_decayItem = nullptr;
    OptionsCardItem *m_decayRatioItem = nullptr;
    OptionsCardItem *m_releaseItem = nullptr;
    OptionsCardItem *m_frequencyItem = nullptr;
    OptionsCardItem *m_adjustByProjectItem = nullptr;
    OptionsCardItem *m_controlItem = nullptr;
};



#endif // MIDIPAGE_H
