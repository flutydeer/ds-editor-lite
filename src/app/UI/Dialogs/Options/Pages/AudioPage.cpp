#include "AudioPage.h"

#include <QApplication>
#include <QBoxLayout>
#include <QPushButton>
#include <QFormLayout>
#include <QCheckBox>
#include <QMessageBox>
#include <QLabel>
#include <QSettings>
#include <QEvent>
#include <QSignalBlocker>

#include <TalcsCore/Decibels.h>
#include <TalcsDevice/AudioDriverManager.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsDevice/SDLAudioDriverDisplayNameHelper.h>

#include "Utils/Decibellinearizer.h"
#include "UI/Controls/SvsSeekbar.h"
#include "UI/Controls/SvsExpressionSpinBox.h"
#include "UI/Controls/SvsExpressionDoubleSpinBox.h"
#include "UI/Controls/ComboBox.h"

#include <Model/AppOptions/AppOptions.h>
#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/AudioContext.h>
#include <Modules/Audio/utils/DeviceTester.h>
#include <Modules/Audio/AudioSettings.h>

#include "UI/Controls/OptionListCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include "Utils/SystemUtils.h"

#include <QDesktopServices>

static double sliderValueToGain(const double sliderValue) {
    return talcs::Decibels::decibelsToGain(DecibelLinearizer::linearValueToDecibel(sliderValue));
}

static double gainToSliderValue(const float gain) {
    return DecibelLinearizer::decibelToLinearValue(talcs::Decibels::gainToDecibels(gain));
}

static double sliderValueToPan(const int sliderValue) {
    return sliderValue / 100.0;
}

static int panToSliderValue(const double pan) {
    return static_cast<int>(round(pan * 100.0));
}

enum PlayheadBehaviorId {
    ReturnToStart = 0,
    KeepAtCurrent = 1,
    KeepAtCurrentButPlayFromStart = 2,
};

QWidget *AudioPage::createContentWidget() {
    const auto widget = new QWidget;
    const auto mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins({});

    m_driverComboBox = new ComboBox;
    m_deviceComboBox = new ComboBox;
    m_testDeviceButton = new QPushButton(tr("&Test"));
    m_deviceControlPanelButton = new QPushButton(tr("Control &Panel"));
    m_bufferSizeComboBox = new ComboBox;
    m_sampleRateComboBox = new ComboBox;
    m_hotPlugModeComboBox = new ComboBox;
    m_hotPlugModeComboBox->addItem(tr("Notify when any device added or removed"),
                                   talcs::OutputContext::Omni);
    m_hotPlugModeComboBox->addItem(tr("Notify when current device removed"),
                                   talcs::OutputContext::Current);
    m_hotPlugModeComboBox->addItem(tr("Do not notify"), talcs::OutputContext::None);

    m_deviceGainSlider = new SVS::SeekBar;
    m_deviceGainSlider->setFixedWidth(256);
    m_deviceGainSlider->setRange(DecibelLinearizer::decibelToLinearValue(-96),
                                 DecibelLinearizer::decibelToLinearValue(6));
    m_deviceGainSlider->setDisplayValueConverter(
        [](const double v) { return DecibelLinearizer::linearValueToDecibel(v); });
    m_deviceGainSpinBox = new SVS::ExpressionDoubleSpinBox;
    m_deviceGainSpinBox->setDecimals(1);
    m_deviceGainSpinBox->setRange(-96, 6);
    m_deviceGainSpinBox->setSpecialValueText("-INF");

    m_devicePanSlider = new SVS::SeekBar;
    m_devicePanSlider->setFixedWidth(256);
    m_devicePanSlider->setRange(-100, 100);
    m_devicePanSlider->setInterval(1);
    m_devicePanSpinBox = new SVS::ExpressionSpinBox;
    m_devicePanSpinBox->setRange(-100, 100);

    m_audioOutputCard = new OptionListCard(tr("Audio Driver and Device"));
    m_driverItem = m_audioOutputCard->addItem(tr("Audio d&river"), m_driverComboBox);
    m_deviceItem = m_audioOutputCard->addItem(
        tr("Audio &device"), {m_deviceComboBox, m_testDeviceButton, m_deviceControlPanelButton});
    m_bufferSizeItem = m_audioOutputCard->addItem(tr("&Buffer size"), m_bufferSizeComboBox);
    m_sampleRateItem = m_audioOutputCard->addItem(tr("&Sample rate"), m_sampleRateComboBox);
    m_hotPlugItem = m_audioOutputCard->addItem(tr("&Hot plug notification"), m_hotPlugModeComboBox);
    m_gainItem = m_audioOutputCard->addItem(tr("Device &Gain (dB)"),
                                            {m_deviceGainSlider, m_deviceGainSpinBox});
    m_panItem =
        m_audioOutputCard->addItem(tr("Device &Pan"), {m_devicePanSlider, m_devicePanSpinBox});
    mainLayout->addWidget(m_audioOutputCard);

    m_playHeadBehaviorComboBox = new ComboBox;
    m_playHeadBehaviorComboBox->addItem(tr("Return to the start position after stopped"),
                                        ReturnToStart);
    m_playHeadBehaviorComboBox->addItem(
        tr("Keep at current position after stopped, and play from current position next time"),
        KeepAtCurrent);
    m_playHeadBehaviorComboBox->addItem(
        tr("Keep at current position after stopped, but play from the start position next time"),
        KeepAtCurrentButPlayFromStart);

    m_playbackCard = new OptionListCard(tr("Playback"));
    m_playheadItem = m_playbackCard->addItem(tr("Playhead behavior"), m_playHeadBehaviorComboBox);
    mainLayout->addWidget(m_playbackCard);

    m_fileBufferingReadAheadSizeSpinBox = new SVS::ExpressionSpinBox;
    m_fileBufferingReadAheadSizeSpinBox->setRange(0, std::numeric_limits<int>::max());
    m_fileCard = new OptionListCard(tr("File Caching"));
    m_fileBufferItem = m_fileCard->addItem(tr("&File reading buffer size (samples)"),
                                           m_fileBufferingReadAheadSizeSpinBox);
    mainLayout->addWidget(m_fileCard);
    mainLayout->addStretch();
    widget->setLayout(mainLayout);
    widget->setContentsMargins({});

    auto outputSys = AudioSystem::outputSystem();

    connect(m_testDeviceButton, &QPushButton::clicked, this, [outputSys, this] {
        if (!outputSys->isReady()) {
            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText(tr("Cannot start audio playback"));
            msgBox.setInformativeText(
                tr("Please check the status of the audio driver and device."));
            msgBox.exec();
        } else {
            DeviceTester::playTestSound();
        }
    });

    connect(m_deviceControlPanelButton, &QPushButton::clicked, this, [] {
        if (const auto device = AudioSystem::outputSystem()->outputContext()->device()) {
            const auto driverName = device->driver()->name();
            if (SystemUtils::isWindows() &&
                (driverName == "winmm" || driverName == "directsound" || driverName == "wasapi")) {
                QDesktopServices::openUrl(QUrl("ms-settings:sound"));
            } else
                device->openControlPanel();
        }
    });

    updateDriverComboBox();

    m_hotPlugModeComboBox->setCurrentIndex(m_hotPlugModeComboBox->findData(
        AudioSystem::outputSystem()->outputContext()->hotPlugNotificationMode()));

    m_deviceGainSlider->setValue(
        gainToSliderValue(outputSys->outputContext()->controlMixer()->gain()));
    connect(m_deviceGainSlider, &SVS::SeekBar::valueChanged, this,
            [this](const double value) { updateGain(sliderValueToGain(value)); });
    m_deviceGainSpinBox->setValue(
        talcs::Decibels::gainToDecibels(outputSys->outputContext()->controlMixer()->gain()));
    connect(m_deviceGainSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](const double value) {
                updateGain(talcs::Decibels::decibelsToGain(static_cast<float>(value)));
            });
    m_devicePanSlider->setValue(
        panToSliderValue(outputSys->outputContext()->controlMixer()->pan()));
    connect(m_devicePanSlider, &SVS::SeekBar::valueChanged, this,
            [this](const int value) { updatePan(sliderValueToPan(value)); });
    m_devicePanSpinBox->setValue(
        panToSliderValue(outputSys->outputContext()->controlMixer()->pan()));
    connect(m_devicePanSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](const int value) { updatePan(sliderValueToPan(value)); });

    m_playHeadBehaviorComboBox->setCurrentIndex(
        m_playHeadBehaviorComboBox->findData(AudioSettings::playheadBehavior()));

    m_fileBufferingReadAheadSizeSpinBox->setValue(AudioSettings::fileBufferingReadAheadSize());
    return widget;
}

void AudioPage::updateDriverComboBox() {
    auto outputSys = AudioSystem::outputSystem();

    auto driverList = outputSys->outputContext()->driverManager()->drivers();
    for (int i = 0; i < driverList.size(); i++) {
        m_driverComboBox->addItem(
            talcs::SDLAudioDriverDisplayNameHelper::getDisplayName(driverList[i]), driverList[i]);
        if (outputSys->outputContext()->driver() &&
            driverList[i] == outputSys->outputContext()->driver()->name())
            m_driverComboBox->setCurrentIndex(i);
    }
    if (!outputSys->outputContext()->driver()) {
        m_driverComboBox->addItem(tr("(Not working)"));
        m_driverComboBox->setCurrentIndex(m_driverComboBox->count() - 1);
    }

    updateDeviceComboBox();

    if (outputSys->outputContext()->driver()) {
        connect(outputSys->outputContext()->driver(), &talcs::AudioDriver::deviceChanged, this,
                [this] {
                    disconnect(m_deviceComboBox, nullptr, this, nullptr);
                    m_deviceComboBox->clear();
                    disconnect(m_bufferSizeComboBox, nullptr, this, nullptr);
                    m_bufferSizeComboBox->clear();
                    disconnect(m_sampleRateComboBox, nullptr, this, nullptr);
                    m_sampleRateComboBox->clear();
                    updateDeviceComboBox();
                });
    }

    connect(
        m_driverComboBox, QOverload<int>::of(&ComboBox::currentIndexChanged), this,
        [outputSys, this](const int index) {
            const auto newDrvName = m_driverComboBox->itemData(index).toString();
            if (newDrvName.isEmpty())
                return;
            disconnect(m_deviceComboBox, nullptr, this, nullptr);
            m_deviceComboBox->clear();
            disconnect(m_bufferSizeComboBox, nullptr, this, nullptr);
            m_bufferSizeComboBox->clear();
            disconnect(m_sampleRateComboBox, nullptr, this, nullptr);
            m_sampleRateComboBox->clear();
            if (!outputSys->setDriver(newDrvName)) {
                QMessageBox::warning(
                    this, {},
                    tr("Cannot initialize %1 driver")
                        .arg(talcs::SDLAudioDriverDisplayNameHelper::getDisplayName(newDrvName)));
                if (m_driverComboBox->itemData(m_driverComboBox->count() - 1).isNull()) {
                    m_driverComboBox->setCurrentIndex(m_driverComboBox->count() - 1);
                } else {
                    m_driverComboBox->addItem(tr("(Not working)"));
                    m_driverComboBox->setCurrentIndex(m_driverComboBox->count() - 1);
                }
            } else {
                if (m_driverComboBox->itemData(m_driverComboBox->count() - 1).isNull()) {
                    m_driverComboBox->removeItem(m_driverComboBox->count() - 1);
                }
                updateDeviceComboBox();
            }
        });
}

void AudioPage::updateDeviceComboBox() {
    auto outputSys = AudioSystem::outputSystem();

    bool currentIndexDetermined = false;

    if (!outputSys->outputContext()->driver()->defaultDevice().isEmpty()) {
        m_deviceComboBox->addItem(tr("Default device"), QString(""));
        if (outputSys->outputContext()->device() &&
            outputSys->outputContext()->device()->name().isEmpty()) {
            currentIndexDetermined = true;
            m_deviceComboBox->setCurrentIndex(0);
        }
    }
    auto deviceList = outputSys->outputContext()->driver()->devices();
    for (int i = 0; i < deviceList.size(); i++) {
        m_deviceComboBox->addItem(deviceList[i], deviceList[i]);
        if (outputSys->outputContext()->device() &&
            deviceList[i] == outputSys->outputContext()->device()->name()) {
            currentIndexDetermined = true;
            m_deviceComboBox->setCurrentIndex(i + 1);
        }
    }
    if (!outputSys->outputContext()->device()) {
        m_deviceComboBox->addItem(tr("(Not working)"));
        m_deviceComboBox->setCurrentIndex(m_deviceComboBox->count() - 1);
    } else if (!currentIndexDetermined) {
        m_deviceComboBox->addItem(outputSys->outputContext()->device()->name() +
                                  tr("(Not working)"));
        m_deviceComboBox->setCurrentIndex(m_deviceComboBox->count() - 1);
    }

    if (outputSys->outputContext()->device()) {
        updateBufferSizeAndSampleRateComboBox();
    }

    connect(m_deviceComboBox, QOverload<int>::of(&ComboBox::currentIndexChanged), this,
            [outputSys, this](const int index) {
                if (m_deviceComboBox->itemData(index).isNull())
                    return;
                const auto newDevName = m_deviceComboBox->itemData(index).toString();
                if (!outputSys->setDevice(newDevName)) {
                    for (int i = 0; i < m_deviceComboBox->count(); i++) {
                        if ((!outputSys->outputContext()->device() &&
                             m_deviceComboBox->itemData(i).isNull()) ||
                            (outputSys->outputContext()->device() &&
                             outputSys->outputContext()->device()->name() ==
                                 m_deviceComboBox->itemData(i).toString())) {
                            QSignalBlocker blocker(m_deviceComboBox);
                            m_deviceComboBox->setCurrentIndex(i);
                            break;
                        }
                    }
                    QMessageBox::warning(
                        this, {},
                        tr("Audio device %1 is not available")
                            .arg(newDevName.isEmpty() ? tr("Default device") : newDevName));
                } else {
                    if (m_deviceComboBox->itemData(m_deviceComboBox->count() - 1).isNull()) {
                        m_deviceComboBox->removeItem(m_deviceComboBox->count() - 1);
                    }
                    disconnect(m_bufferSizeComboBox, nullptr, this, nullptr);
                    m_bufferSizeComboBox->clear();
                    disconnect(m_sampleRateComboBox, nullptr, this, nullptr);
                    m_sampleRateComboBox->clear();
                    updateBufferSizeAndSampleRateComboBox();
                }
            });
}

void AudioPage::updateBufferSizeAndSampleRateComboBox() {
    auto outputSys = AudioSystem::outputSystem();

    auto bufferSizeList = outputSys->outputContext()->device()->availableBufferSizes();
    for (int i = 0; i < bufferSizeList.size(); i++) {
        m_bufferSizeComboBox->addItem(QString::number(bufferSizeList[i]), bufferSizeList[i]);
        if (bufferSizeList[i] == outputSys->outputContext()->adoptedBufferSize())
            m_bufferSizeComboBox->setCurrentIndex(i);
    }
    connect(m_bufferSizeComboBox, QOverload<int>::of(&ComboBox::currentIndexChanged), this,
            [outputSys, this](const int index) {
                const auto newBufferSize = m_bufferSizeComboBox->itemData(index).value<qint64>();
                outputSys->setAdoptedBufferSize(newBufferSize);
            });
    auto sampleRateList = outputSys->outputContext()->device()->availableSampleRates();
    for (int i = 0; i < sampleRateList.size(); i++) {
        m_sampleRateComboBox->addItem(QString::number(sampleRateList[i]), sampleRateList[i]);
        if (sampleRateList[i] == outputSys->outputContext()->adoptedSampleRate())
            m_sampleRateComboBox->setCurrentIndex(i);
    }
    connect(m_sampleRateComboBox, QOverload<int>::of(&ComboBox::currentIndexChanged), this,
            [outputSys, this](const int index) {
                const auto newSampleRate = m_sampleRateComboBox->itemData(index).value<double>();
                outputSys->setAdoptedSampleRate(newSampleRate);
            });
}

void AudioPage::updateGain(const double gain) const {
    QSignalBlocker sliderBlocker(m_deviceGainSlider);
    QSignalBlocker spinBoxBlocker(m_deviceGainSpinBox);

    m_deviceGainSlider->setValue(gainToSliderValue(static_cast<float>(gain)));
    m_deviceGainSpinBox->setValue(talcs::Decibels::gainToDecibels(static_cast<float>(gain)));
    AudioSystem::outputSystem()->outputContext()->controlMixer()->setGain(static_cast<float>(gain));
}

void AudioPage::updatePan(const double pan) const {
    QSignalBlocker sliderBlocker(m_devicePanSlider);
    QSignalBlocker spinBoxBlocker(m_devicePanSpinBox);

    m_devicePanSlider->setValue(panToSliderValue(pan));
    m_devicePanSpinBox->setValue(panToSliderValue(pan));
    AudioSystem::outputSystem()->outputContext()->controlMixer()->setPan(static_cast<float>(pan));
}

AudioPage::AudioPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

AudioPage::~AudioPage() {
    AudioPage::modifyOption();
}

void AudioPage::modifyOption() {
    AudioSystem::outputSystem()->setHotPlugNotificationMode(
        static_cast<talcs::OutputContext::HotPlugNotificationMode>(
            m_hotPlugModeComboBox->currentData().toInt()));
    AudioSystem::outputSystem()->setFileBufferingReadAheadSize(
        m_fileBufferingReadAheadSizeSpinBox->value());
    AudioSettings::setDeviceGain(
        AudioSystem::outputSystem()->outputContext()->controlMixer()->gain());
    AudioSettings::setDevicePan(
        AudioSystem::outputSystem()->outputContext()->controlMixer()->pan());
    AudioSettings::setFileBufferingReadAheadSize(m_fileBufferingReadAheadSizeSpinBox->value());
    AudioSettings::setPlayheadBehavior(m_playHeadBehaviorComboBox->currentData().toInt());
    appOptions->saveAndNotify(AppOptionsGlobal::Audio);
}

void AudioPage::changeEvent(QEvent *event) {
    QScrollArea::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void AudioPage::retranslateUi() {
    m_audioOutputCard->setTitle(tr("Audio Driver and Device"));
    m_driverItem->setTitle(tr("Audio d&river"));
    m_deviceItem->setTitle(tr("Audio &device"));
    m_bufferSizeItem->setTitle(tr("&Buffer size"));
    m_sampleRateItem->setTitle(tr("&Sample rate"));
    m_hotPlugItem->setTitle(tr("&Hot plug notification"));
    m_gainItem->setTitle(tr("Device &Gain (dB)"));
    m_panItem->setTitle(tr("Device &Pan"));
    m_testDeviceButton->setText(tr("&Test"));
    m_deviceControlPanelButton->setText(tr("Control &Panel"));

    const auto hotPlugMode = m_hotPlugModeComboBox->currentData();
    const QSignalBlocker hotPlugBlocker(m_hotPlugModeComboBox);
    m_hotPlugModeComboBox->clear();
    m_hotPlugModeComboBox->addItem(tr("Notify when any device added or removed"),
                                   talcs::OutputContext::Omni);
    m_hotPlugModeComboBox->addItem(tr("Notify when current device removed"),
                                   talcs::OutputContext::Current);
    m_hotPlugModeComboBox->addItem(tr("Do not notify"), talcs::OutputContext::None);
    m_hotPlugModeComboBox->setCurrentIndex(m_hotPlugModeComboBox->findData(hotPlugMode));

    const auto playheadBehavior = m_playHeadBehaviorComboBox->currentData();
    const QSignalBlocker playheadBlocker(m_playHeadBehaviorComboBox);
    m_playHeadBehaviorComboBox->clear();
    m_playHeadBehaviorComboBox->addItem(tr("Return to the start position after stopped"),
                                        ReturnToStart);
    m_playHeadBehaviorComboBox->addItem(
        tr("Keep at current position after stopped, and play from current position next time"),
        KeepAtCurrent);
    m_playHeadBehaviorComboBox->addItem(
        tr("Keep at current position after stopped, but play from the start position next time"),
        KeepAtCurrentButPlayFromStart);
    m_playHeadBehaviorComboBox->setCurrentIndex(
        m_playHeadBehaviorComboBox->findData(playheadBehavior));

    for (int i = 0; i < m_driverComboBox->count(); ++i) {
        if (m_driverComboBox->itemData(i).isNull())
            m_driverComboBox->setItemText(i, tr("(Not working)"));
    }
    for (int i = 0; i < m_deviceComboBox->count(); ++i) {
        if (!m_deviceComboBox->itemData(i).isNull() &&
            m_deviceComboBox->itemData(i).toString().isEmpty()) {
            m_deviceComboBox->setItemText(i, tr("Default device"));
        } else if (m_deviceComboBox->itemData(i).isNull()) {
            const auto device = AudioSystem::outputSystem()->outputContext()->device();
            m_deviceComboBox->setItemText(i, device ? device->name() + tr("(Not working)")
                                                    : tr("(Not working)"));
        }
    }

    m_playbackCard->setTitle(tr("Playback"));
    m_playheadItem->setTitle(tr("Playhead behavior"));
    m_fileCard->setTitle(tr("File Caching"));
    m_fileBufferItem->setTitle(tr("&File reading buffer size (samples)"));
}
