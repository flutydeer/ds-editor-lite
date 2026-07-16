//
// Created by fluty on 24-3-18.
//

#ifndef AUDIOPAGE_H
#define AUDIOPAGE_H

#include "IOptionPage.h"
class ComboBox;
class OptionListCard;
class OptionsCardItem;
class QPushButton;

namespace SVS {
    class SeekBar;
    class ExpressionDoubleSpinBox;
    class ExpressionSpinBox;
}

class AudioPage : public IOptionPage {
    Q_OBJECT

public:
    explicit AudioPage(QWidget *parent = nullptr);
    ~AudioPage() override;

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;
    void changeEvent(QEvent *event) override;

private:
    void retranslateUi();
    void updateDriverComboBox();
    void updateDeviceComboBox();
    void updateBufferSizeAndSampleRateComboBox();
    void updateGain(double gain) const;
    void updatePan(double pan) const;

    ComboBox *m_driverComboBox = nullptr;
    ComboBox *m_deviceComboBox = nullptr;
    ComboBox *m_bufferSizeComboBox = nullptr;
    ComboBox *m_sampleRateComboBox = nullptr;
    ComboBox *m_hotPlugModeComboBox = nullptr;
    SVS::SeekBar *m_deviceGainSlider = nullptr;
    SVS::ExpressionDoubleSpinBox *m_deviceGainSpinBox = nullptr;
    SVS::SeekBar *m_devicePanSlider = nullptr;
    SVS::ExpressionSpinBox *m_devicePanSpinBox = nullptr;
    ComboBox *m_playHeadBehaviorComboBox = nullptr;
    SVS::ExpressionSpinBox *m_fileBufferingReadAheadSizeSpinBox = nullptr;

    QPushButton *m_testDeviceButton = nullptr;
    QPushButton *m_deviceControlPanelButton = nullptr;
    OptionListCard *m_audioOutputCard = nullptr;
    OptionListCard *m_playbackCard = nullptr;
    OptionListCard *m_fileCard = nullptr;
    OptionsCardItem *m_driverItem = nullptr;
    OptionsCardItem *m_deviceItem = nullptr;
    OptionsCardItem *m_bufferSizeItem = nullptr;
    OptionsCardItem *m_sampleRateItem = nullptr;
    OptionsCardItem *m_hotPlugItem = nullptr;
    OptionsCardItem *m_gainItem = nullptr;
    OptionsCardItem *m_panItem = nullptr;
    OptionsCardItem *m_playheadItem = nullptr;
    OptionsCardItem *m_fileBufferItem = nullptr;
};


#endif // AUDIOPAGE_H
