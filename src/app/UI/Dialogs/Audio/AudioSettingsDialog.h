#ifndef DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H
#define DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H

#include "Modules/Audio/AudioSystem.h"
#include "UI/Dialogs/Base/Dialog.h"

class ComboBox;
class QCheckBox;
class QDoubleSpinBox;

class AudioSettingsDialog : public Dialog {
    Q_OBJECT
public:
    explicit AudioSettingsDialog(QWidget *parent = nullptr);

    [[nodiscard]] AudioSystem::HotPlugMode hotPlugMode() const;
    void setHotPlugMode(AudioSystem::HotPlugMode mode);

    [[nodiscard]] bool closeDeviceAtBackground() const;
    void setCloseDeviceAtBackground(bool enabled);

    [[nodiscard]] bool closeDeviceOnPlaybackStop() const;
    void setCloseDeviceOnPlaybackStop(bool enabled);

    [[nodiscard]] double fileBufferingSizeMsec() const;
    void setFileBufferingSizeMsec(double value);

private:
    ComboBox *m_driverComboBox;
    ComboBox *m_deviceComboBox;
    ComboBox *m_bufferSizeComboBox;
    ComboBox *m_sampleRateComboBox;
    ComboBox *m_hotPlugModeComboBox;
    QCheckBox *m_closeDeviceAtBackgroundCheckBox;
    QCheckBox *m_closeDeviceOnPlaybackStopCheckBox;
    QDoubleSpinBox *m_fileBufferingSizeMsec;

    void updateDeviceComboBox();
    void updateBufferSizeAndSampleRateComboBox();
    void updateDriverComboBox();

    void updateOptionsDisplay();
    void applySetting() const;
};



#endif // DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H
