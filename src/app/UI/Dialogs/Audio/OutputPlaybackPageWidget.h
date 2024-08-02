#ifndef OUTPUTPLAYBACKPAGEWIDGET_H
#define OUTPUTPLAYBACKPAGEWIDGET_H

#include <QWidget>

class QComboBox;
class QCheckBox;

namespace SVS {
    class SeekBar;
    class ExpressionSpinBox;
    class ExpressionDoubleSpinBox;
}

class OutputPlaybackPageWidget : public QWidget {
    Q_OBJECT
public:
    explicit OutputPlaybackPageWidget(QWidget *parent = nullptr);
    void accept() const;

private:
    QComboBox *m_driverComboBox = nullptr;
    QComboBox *m_deviceComboBox = nullptr;
    QComboBox *m_bufferSizeComboBox = nullptr;
    QComboBox *m_sampleRateComboBox = nullptr;
    QComboBox *m_hotPlugModeComboBox = nullptr;
    SVS::SeekBar *m_deviceGainSlider = nullptr;
    SVS::ExpressionDoubleSpinBox *m_deviceGainSpinBox = nullptr;
    SVS::SeekBar *m_devicePanSlider = nullptr;
    SVS::ExpressionSpinBox *m_devicePanSpinBox = nullptr;
    QComboBox *m_playHeadBehaviorComboBox = nullptr;
    QCheckBox *m_closeDeviceOnPlaybackStopCheckBox = nullptr;
    SVS::ExpressionSpinBox *m_fileBufferingReadAheadSizeSpinBox = nullptr;

    void updateDriverComboBox();
    void updateDeviceComboBox();
    void updateBufferSizeAndSampleRateComboBox();

    void updateGain(double gain);
    void updatePan(double pan);
};



#endif // OUTPUTPLAYBACKPAGEWIDGET_H
