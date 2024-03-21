//
// Created by fluty on 24-3-18.
//

#ifndef AUDIOPAGE_H
#define AUDIOPAGE_H

#include "IOptionPage.h"
#include "Modules/Audio/AudioSystem.h"

class OptionsCardItem;
class ComboBox;
class QCheckBox;
class QDoubleSpinBox;

class AudioPage : public IOptionPage {
    Q_OBJECT

public:
    explicit AudioPage(QWidget *parent = nullptr);

    [[nodiscard]] AudioSystem::HotPlugMode hotPlugMode() const;
    void setHotPlugMode(AudioSystem::HotPlugMode mode);

    [[nodiscard]] bool closeDeviceAtBackground() const;
    void setCloseDeviceAtBackground(bool enabled);

    [[nodiscard]] bool closeDeviceOnPlaybackStop() const;
    void setCloseDeviceOnPlaybackStop(bool enabled);

    [[nodiscard]] double fileBufferingSizeMsec() const;
    void setFileBufferingSizeMsec(double value);

protected:
    void modifyOption() override;

private:
    ComboBox *m_driverComboBox;
    ComboBox *m_deviceComboBox;
    ComboBox *m_bufferSizeComboBox;
    ComboBox *m_sampleRateComboBox;
    ComboBox *m_hotPlugModeComboBox;
    OptionsCardItem *m_closeDeviceAtBackgroundItem;
    OptionsCardItem *m_closeDeviceOnPlaybackStopItem;
    QDoubleSpinBox *m_fileBufferingSizeMsec;

    void updateDeviceComboBox();
    void updateBufferSizeAndSampleRateComboBox();
    void updateDriverComboBox();

    void updateOptionsDisplay();
};



#endif // AUDIOPAGE_H
