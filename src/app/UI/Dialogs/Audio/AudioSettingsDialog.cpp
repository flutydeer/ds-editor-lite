#include "AudioSettingsDialog.h"

#include <QApplication>
#include <QBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QLabel>

#include <TalcsDevice/AudioDriverManager.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsRemote/RemoteAudioDevice.h>

#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/AudioContext.h"

#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"

void AudioSettingsDialog::updateDeviceComboBox() {
    auto deviceList = AudioSystem::instance()->driver()->devices();
    for (int i = 0; i < deviceList.size(); i++) {
        if (!AudioSystem::instance()->device()) {
            m_deviceComboBox->addItem(deviceList[i] + tr(" (Not working)"), deviceList[i]);
        } else {
            if (deviceList[i] == AudioSystem::instance()->device()->name()) {
                if (AudioSystem::instance()->isDeviceAutoClosed())
                    m_deviceComboBox->addItem(deviceList[i] + tr(" (Not working)"), deviceList[i]);
                else
                    m_deviceComboBox->addItem(deviceList[i], deviceList[i]);
                m_deviceComboBox->setCurrentIndex(i);
            } else {
                m_deviceComboBox->addItem(deviceList[i], deviceList[i]);
            }
        }
    }

    if (AudioSystem::instance()->device()) {
        updateBufferSizeAndSampleRateComboBox();
    }

    connect(m_deviceComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        disconnect(m_bufferSizeComboBox, nullptr, this, nullptr);
        m_bufferSizeComboBox->clear();
        disconnect(m_sampleRateComboBox, nullptr, this, nullptr);
        m_sampleRateComboBox->clear();
        auto newDevName = m_deviceComboBox->itemData(index).toString();
        if (!AudioSystem::instance()->setDevice(newDevName)) {
            QMessageBox::warning(this, {}, tr("Audio device %1 is not available!").arg(newDevName));
            m_deviceComboBox->setItemText(index, newDevName + tr(" (Not working)"));
        } else {
            updateBufferSizeAndSampleRateComboBox();
        }
    });
}
void AudioSettingsDialog::updateBufferSizeAndSampleRateComboBox() {
    auto bufferSizeList = AudioSystem::instance()->device()->availableBufferSizes();
    for (int i = 0; i < bufferSizeList.size(); i++) {
        m_bufferSizeComboBox->addItem(QString::number(bufferSizeList[i]), bufferSizeList[i]);
        if (bufferSizeList[i] == AudioSystem::instance()->adoptedBufferSize())
            m_bufferSizeComboBox->setCurrentIndex(i);
    }
    connect(m_bufferSizeComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        auto newBufferSize = m_bufferSizeComboBox->itemData(index).value<qint64>();
        AudioSystem::instance()->setAdoptedBufferSize(newBufferSize);
    });
    auto sampleRateList = AudioSystem::instance()->device()->availableSampleRates();
    for (int i = 0; i < sampleRateList.size(); i++) {
        m_sampleRateComboBox->addItem(QString::number(sampleRateList[i]), sampleRateList[i]);
        if (sampleRateList[i] == AudioSystem::instance()->adoptedSampleRate())
            m_sampleRateComboBox->setCurrentIndex(i);
    }
    connect(m_sampleRateComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        auto newSampleRate = m_sampleRateComboBox->itemData(index).value<double>();
        AudioSystem::instance()->setAdoptedSampleRate(newSampleRate);
    });
}

AudioSettingsDialog::AudioSettingsDialog(QWidget *parent) : Dialog(parent) {
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowTitle(tr("Audio Settings"));
    auto mainLayout = new QVBoxLayout;

    auto audioOutputGroupBox = new QGroupBox(tr("Audio Output Options"));
    auto audioOutputLayout = new QFormLayout;
    m_driverComboBox = new ComboBox;
    audioOutputLayout->addRow(tr("Audio Driver"), m_driverComboBox);
    auto deviceLayout = new QHBoxLayout;
    m_deviceComboBox = new ComboBox;
    deviceLayout->addWidget(m_deviceComboBox);
    auto testDeviceButton = new Button(tr("Test"));
    deviceLayout->addWidget(testDeviceButton);
    audioOutputLayout->addRow(tr("Audio Device"), deviceLayout);
    auto deviceControlPanelButton = new Button(tr("Device Control Panel"));
    audioOutputLayout->addWidget(deviceControlPanelButton);
    m_bufferSizeComboBox = new ComboBox;
    audioOutputLayout->addRow(tr("Buffer Size"), m_bufferSizeComboBox);
    m_sampleRateComboBox = new ComboBox;
    audioOutputLayout->addRow(tr("Sample Rate"), m_sampleRateComboBox);
    m_hotPlugModeComboBox = new ComboBox;
    m_hotPlugModeComboBox->addItems({tr("Notify any device change"), tr("Notify current device removal"), tr("Do not notify")});
    audioOutputLayout->addRow(tr("Hot Plug Mode"), m_hotPlugModeComboBox);
    audioOutputGroupBox->setLayout(audioOutputLayout);
    mainLayout->addWidget(audioOutputGroupBox);

    auto playbackGroupBox = new QGroupBox(tr("Playback Options"));
    auto playbackLayout = new QFormLayout;
    m_closeDeviceAtBackgroundCheckBox = new QCheckBox(tr("Close audio device when %1 is in the background").arg(qApp->applicationDisplayName()));
    playbackLayout->addRow(m_closeDeviceAtBackgroundCheckBox);
    m_closeDeviceOnPlaybackStopCheckBox = new QCheckBox(tr("Close audio device when playback is stopped"));
    playbackLayout->addRow(m_closeDeviceOnPlaybackStopCheckBox);
    playbackGroupBox->setLayout(playbackLayout);
    mainLayout->addWidget(playbackGroupBox);

    auto fileGroupBox = new QGroupBox(tr("File Options"));
    auto fileLayout = new QFormLayout;
    m_fileBufferingSizeMsec = new QDoubleSpinBox;
    m_fileBufferingSizeMsec->setRange(0.0, std::numeric_limits<double>::max());
    m_fileBufferingSizeMsec->setSuffix(" ms");
    fileLayout->addRow(tr("Buffer size when reading from file"), m_fileBufferingSizeMsec);
    fileGroupBox->setLayout(fileLayout);
    mainLayout->addWidget(fileGroupBox);

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    auto okButton = new Button(tr("OK"));
    okButton->setPrimary(true);
    okButton->setDefault(true);
    buttonLayout->addWidget(okButton);
    auto cancelButton = new Button(tr("Cancel"));
    buttonLayout->addWidget(cancelButton);
    auto applyButton = new Button(tr("Apply"));
    buttonLayout->addWidget(applyButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

    connect(okButton, &QPushButton::clicked, this, &AudioSettingsDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &AudioSettingsDialog::reject);
    connect(applyButton, &QPushButton::clicked, this, &AudioSettingsDialog::applySetting);
    connect(this, &AudioSettingsDialog::accepted, this, &AudioSettingsDialog::applySetting);

    connect(testDeviceButton, &QPushButton::clicked, this, [=] {
        AudioSystem::instance()->testDevice();
    });

    connect(deviceControlPanelButton, &QPushButton::clicked, this, [=] {
        if (AudioSystem::instance()->device())
            AudioSystem::instance()->device()->openControlPanel();
    });

    if (AudioSystem::instance()->socket()) {
        audioOutputLayout->insertRow(0, new QLabel(tr("These options are disabled in plugged mode.")));
        m_driverComboBox->setDisabled(true);
        m_deviceComboBox->addItem(AudioSystem::instance()->device()->name());
        m_deviceComboBox->setDisabled(true);
        m_bufferSizeComboBox->addItem(QString::number(AudioSystem::instance()->device()->bufferSize()));
        m_bufferSizeComboBox->setDisabled(true);
        m_sampleRateComboBox->addItem(QString::number(AudioSystem::instance()->device()->sampleRate()));
        m_sampleRateComboBox->setDisabled(true);
        m_hotPlugModeComboBox->setDisabled(true);
        connect(dynamic_cast<talcs::RemoteAudioDevice *>(AudioSystem::instance()->device()), &talcs::RemoteAudioDevice::remoteOpened, this, [=](qint64 bufferSize, double sampleRate) {
            m_bufferSizeComboBox->clear();
            m_sampleRateComboBox->clear();
            m_bufferSizeComboBox->addItem(QString::number(bufferSize));
            m_sampleRateComboBox->addItem(QString::number(sampleRate));
        });
    } else {
        updateDriverComboBox();
    }

    updateOptionsDisplay();
}
void AudioSettingsDialog::updateDriverComboBox() {
    if (!AudioSystem::instance()->driver())
        AudioSystem::instance()->findProperDriver();
    if (!AudioSystem::instance()->driver()) {
        QMessageBox::warning(this, {}, tr("No audio driver available on this computer!"));
    } else {

        auto driverList = AudioSystem::instance()->driverManager()->drivers();
        for (int i = 0; i < driverList.size(); i++) {
            m_driverComboBox->addItem(AudioSystem::driverDisplayName(driverList[i]), driverList[i]);
            if (driverList[i] == AudioSystem::instance()->driver()->name())
                m_driverComboBox->setCurrentIndex(i);
        }

        if (!AudioSystem::instance()->device())
            AudioSystem::instance()->findProperDevice();
        if (!AudioSystem::instance()->device())
            QMessageBox::warning(this, {}, tr("No audio device available in driver mode %1!").arg(AudioSystem::driverDisplayName(AudioSystem::instance()->driver()->name())));

        updateDeviceComboBox();
        connect(AudioSystem::instance()->driver(), &talcs::AudioDriver::deviceChanged, this, [=] {
            disconnect(m_deviceComboBox, nullptr, this, nullptr);
            m_deviceComboBox->clear();
            disconnect(m_bufferSizeComboBox, nullptr, this, nullptr);
            m_bufferSizeComboBox->clear();
            disconnect(m_sampleRateComboBox, nullptr, this, nullptr);
            m_sampleRateComboBox->clear();
            updateDeviceComboBox();
        });
    }

    connect(m_driverComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        disconnect(m_deviceComboBox, nullptr, this, nullptr);
        m_deviceComboBox->clear();
        disconnect(m_bufferSizeComboBox, nullptr, this, nullptr);
        m_bufferSizeComboBox->clear();
        disconnect(m_sampleRateComboBox, nullptr, this, nullptr);
        m_sampleRateComboBox->clear();
        auto newDrvName = m_driverComboBox->itemData(index).toString();
        if (!AudioSystem::instance()->setDriver(newDrvName)) {
            QMessageBox::warning(this, {}, tr("Driver mode %1 cannot initialize!").arg(AudioSystem::driverDisplayName(AudioSystem::instance()->driver()->name())));
        } else {
            updateDeviceComboBox();
        }
    });
}

AudioSystem::HotPlugMode AudioSettingsDialog::hotPlugMode() const {
    return AudioSystem::HotPlugMode(m_hotPlugModeComboBox->currentIndex());
}

void AudioSettingsDialog::setHotPlugMode(AudioSystem::HotPlugMode mode) {
    m_hotPlugModeComboBox->setCurrentIndex(mode);
}

bool AudioSettingsDialog::closeDeviceAtBackground() const {
    return m_closeDeviceAtBackgroundCheckBox->isChecked();
}

void AudioSettingsDialog::setCloseDeviceAtBackground(bool enabled) {
    m_closeDeviceAtBackgroundCheckBox->setChecked(enabled);
}

bool AudioSettingsDialog::closeDeviceOnPlaybackStop() const {
    return m_closeDeviceOnPlaybackStopCheckBox->isChecked();
}

void AudioSettingsDialog::setCloseDeviceOnPlaybackStop(bool enabled) {
    m_closeDeviceOnPlaybackStopCheckBox->setChecked(enabled);
}

double AudioSettingsDialog::fileBufferingSizeMsec() const {
    return m_fileBufferingSizeMsec->value();
}

void AudioSettingsDialog::setFileBufferingSizeMsec(double value) {
    m_fileBufferingSizeMsec->setValue(value);
}

void AudioSettingsDialog::updateOptionsDisplay() {
    QSettings settings;
    settings.beginGroup("audio");
    setHotPlugMode(settings.value("hotPlugMode", AudioSystem::NotifyOnAnyChange).value<AudioSystem::HotPlugMode>());
    setCloseDeviceAtBackground(settings.value("closeDeviceAtBackground", false).toBool());
    setCloseDeviceOnPlaybackStop(settings.value("closeDeviceOnPlaybackStop", false).toBool());
    setFileBufferingSizeMsec(settings.value("fileBufferingSizeMsec", 1000.0).toDouble());
}

void AudioSettingsDialog::applySetting() const {
    QSettings settings;
    settings.beginGroup("audio");
    settings.setValue("hotPlugMode", hotPlugMode());
    settings.setValue("closeDeviceAtBackground", closeDeviceAtBackground());
    settings.setValue("closeDeviceOnPlaybackStop", closeDeviceOnPlaybackStop());
    if (!qFuzzyCompare(fileBufferingSizeMsec(), settings.value("fileBufferingSizeMsec", 1000.0).toDouble())) {
        settings.setValue("fileBufferingSizeMsec", fileBufferingSizeMsec());
        AudioSystem::instance()->audioContext()->handleFileBufferingSizeChange();
    }
}
