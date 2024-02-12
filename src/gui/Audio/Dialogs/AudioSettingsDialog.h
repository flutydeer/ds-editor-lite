#ifndef DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H
#define DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H

#include <QDialog>

#include "Audio/AudioSystem.h"

class QComboBox;
class QCheckBox;

namespace QXSpinBox {
    class QExpressionDoubleSpinBox;
}


class AudioSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AudioSettingsDialog(QWidget *parent = nullptr);

    AudioSystem::HotPlugMode hotPlugMode() const;
    void setHotPlugMode(AudioSystem::HotPlugMode mode);

    bool closeDeviceAtBackground() const;
    void setCloseDeviceAtBackground(bool enabled);

    bool closeDeviceOnPlaybackStop() const;
    void setCloseDeviceOnPlaybackStop(bool enabled);

    double fileBufferingSizeMsec() const;
    void setFileBufferingSizeMsec(double value);

private:
    QComboBox *m_driverComboBox;
    QComboBox *m_deviceComboBox;
    QComboBox *m_bufferSizeComboBox;
    QComboBox *m_sampleRateComboBox;
    QComboBox *m_hotPlugModeComboBox;
    QCheckBox *m_closeDeviceAtBackgroundCheckBox;
    QCheckBox *m_closeDeviceOnPlaybackStopCheckBox;
    QXSpinBox::QExpressionDoubleSpinBox *m_fileBufferingSizeMsec;

    void updateDeviceComboBox();
    void updateBufferSizeAndSampleRateComboBox();
    void updateDriverComboBox();

    void updateOptionsDisplay();
    void applySetting();
};



#endif // DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H
