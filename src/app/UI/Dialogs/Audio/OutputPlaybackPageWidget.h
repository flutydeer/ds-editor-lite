#ifndef OUTPUTPLAYBACKPAGEWIDGET_H
#define OUTPUTPLAYBACKPAGEWIDGET_H

#include <QWidget>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;

class SeekBar;

namespace SVS {
    using SeekBar = ::SeekBar;
    using ExpressionSpinBox = ::QSpinBox;
    using ExpressionDoubleSpinBox = ::QDoubleSpinBox;

    class DecibelLinearizer {
    public:
        inline static double decibelToLinearValue(double decibel, double factor = -24) {
            return std::exp((decibel - factor) / - factor) - std::exp(1);
        }

        inline static double linearValueToDecibel(double linearValue, double factor = -24) {
            return -factor * std::log(linearValue + std::exp(1)) + factor;
        }
    };
}

class OutputPlaybackPageWidget : public QWidget {
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
