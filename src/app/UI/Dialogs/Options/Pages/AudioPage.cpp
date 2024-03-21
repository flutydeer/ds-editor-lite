//
// Created by fluty on 24-3-18.
//

#include "AudioPage.h"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPushButton>

#include "UI/Controls/ComboBox.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioContext.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include <TalcsDevice/AudioDriverManager.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsRemote/RemoteAudioDevice.h>

AudioPage::AudioPage(QWidget *parent) : IOptionPage(parent) {
    auto options = AppOptions::instance()->audio();

    auto audioOutputCard = new OptionsCard;
    audioOutputCard->setTitle(tr("Audio Output Options"));

    m_driverComboBox = new ComboBox;
    auto driverItem = new OptionsCardItem;
    driverItem->setTitle(tr("Audio Driver"));
    driverItem->addWidget(m_driverComboBox);

    m_deviceComboBox = new ComboBox;
    auto testDeviceButton = new QPushButton(tr("Test"));
    auto deviceControlPanelButton = new QPushButton(tr("Device Control Panel"));
    auto deviceItem = new OptionsCardItem;
    deviceItem->setTitle(tr("Audio Device"));
    deviceItem->addWidget(m_deviceComboBox);
    deviceItem->addWidget(testDeviceButton);
    deviceItem->addWidget(deviceControlPanelButton);

    m_bufferSizeComboBox = new ComboBox;
    auto bufferSizeItem = new OptionsCardItem;
    bufferSizeItem->setTitle(tr("Buffer Size"));
    bufferSizeItem->addWidget(m_bufferSizeComboBox);

    m_sampleRateComboBox = new ComboBox;
    auto sampleRateItem = new OptionsCardItem;
    sampleRateItem->setTitle(tr("Sample Rate"));
    sampleRateItem->addWidget(m_sampleRateComboBox);

    m_hotPlugModeComboBox = new ComboBox;
    m_hotPlugModeComboBox->addItems(
        {tr("Notify any device change"), tr("Notify current device removal"), tr("Do not notify")});
    auto hotPlugItem = new OptionsCardItem;
    hotPlugItem->setTitle(tr("Hot Plug Mode"));
    hotPlugItem->addWidget(m_hotPlugModeComboBox);

    auto audioOutputCardLayout = new QVBoxLayout;
    audioOutputCardLayout->setContentsMargins({});
    audioOutputCardLayout->setSpacing(0);
    audioOutputCardLayout->addWidget(driverItem);
    audioOutputCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    audioOutputCardLayout->addWidget(deviceItem);
    audioOutputCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    audioOutputCardLayout->addWidget(bufferSizeItem);
    audioOutputCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    audioOutputCardLayout->addWidget(sampleRateItem);
    audioOutputCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    audioOutputCardLayout->addWidget(hotPlugItem);
    audioOutputCard->card()->setLayout(audioOutputCardLayout);

    auto playbackCard = new OptionsCard;
    playbackCard->setTitle(tr("Playback Options"));

    m_closeDeviceAtBackgroundItem = new OptionsCardItem;
    m_closeDeviceAtBackgroundItem->setCheckable(true);
    m_closeDeviceAtBackgroundItem->setTitle(
        tr("Close audio device when %1 is in the background").arg(qApp->applicationDisplayName()));

    m_closeDeviceOnPlaybackStopItem = new OptionsCardItem;
    m_closeDeviceOnPlaybackStopItem->setCheckable(true);
    m_closeDeviceOnPlaybackStopItem->setTitle(tr("Close audio device when playback is stopped"));

    auto playbackCardLayout = new QVBoxLayout;
    playbackCardLayout->setContentsMargins({});
    playbackCardLayout->setSpacing(0);
    playbackCardLayout->addWidget(m_closeDeviceAtBackgroundItem);
    playbackCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    playbackCardLayout->addWidget(m_closeDeviceOnPlaybackStopItem);
    playbackCard->card()->setLayout(playbackCardLayout);

    auto fileCard = new OptionsCard;
    fileCard->setTitle(tr("File Options"));

    m_fileBufferingSizeMsec = new QDoubleSpinBox;
    m_fileBufferingSizeMsec->setRange(0.0, std::numeric_limits<double>::max());
    m_fileBufferingSizeMsec->setSuffix(" ms");
    auto fileBufferingItem = new OptionsCardItem;
    fileBufferingItem->setTitle(tr("Buffer size when reading from file"));
    fileBufferingItem->addWidget(m_fileBufferingSizeMsec);

    auto fileCardLayout = new QVBoxLayout;
    fileCardLayout->setContentsMargins({});
    fileCardLayout->setSpacing(0);
    fileCardLayout->addWidget(fileBufferingItem);

    fileCard->card()->setLayout(fileCardLayout);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(audioOutputCard);
    mainLayout->addWidget(playbackCard);
    mainLayout->addWidget(fileCard);
    mainLayout->addSpacerItem(
        new QSpacerItem(8, 4, QSizePolicy::Expanding, QSizePolicy::Expanding));
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
    connect(testDeviceButton, &QPushButton::clicked, this,
            [=] { AudioSystem::instance()->testDevice(); });

    connect(deviceControlPanelButton, &QPushButton::clicked, this, [=] {
        if (AudioSystem::instance()->device())
            AudioSystem::instance()->device()->openControlPanel();
    });

    if (AudioSystem::instance()->socket()) {
        // audioOutputLayout->insertRow(0,
        //                              new QLabel(tr("These options are disabled in plugged
        //                              mode.")));
        audioOutputCard->card()->layout()->addWidget(
            new QLabel(tr("These options are disabled in plugged mode.")));
        m_driverComboBox->setDisabled(true);
        m_deviceComboBox->addItem(AudioSystem::instance()->device()->name());
        m_deviceComboBox->setDisabled(true);
        m_bufferSizeComboBox->addItem(
            QString::number(AudioSystem::instance()->device()->bufferSize()));
        m_bufferSizeComboBox->setDisabled(true);
        m_sampleRateComboBox->addItem(
            QString::number(AudioSystem::instance()->device()->sampleRate()));
        m_sampleRateComboBox->setDisabled(true);
        m_hotPlugModeComboBox->setDisabled(true);
        connect(dynamic_cast<talcs::RemoteAudioDevice *>(AudioSystem::instance()->device()),
                &talcs::RemoteAudioDevice::remoteOpened, this,
                [=](qint64 bufferSize, double sampleRate) {
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
AudioSystem::HotPlugMode AudioPage::hotPlugMode() const {
    return AudioSystem::HotPlugMode(m_hotPlugModeComboBox->currentIndex());
}
void AudioPage::setHotPlugMode(AudioSystem::HotPlugMode mode) {
    m_hotPlugModeComboBox->setCurrentIndex(mode);
}
bool AudioPage::closeDeviceAtBackground() const {
    return m_closeDeviceAtBackgroundItem->isChecked();
}
void AudioPage::setCloseDeviceAtBackground(bool enabled) {
    m_closeDeviceAtBackgroundItem->setChecked(enabled);
}
bool AudioPage::closeDeviceOnPlaybackStop() const {
    return m_closeDeviceOnPlaybackStopItem->isChecked();
}
void AudioPage::setCloseDeviceOnPlaybackStop(bool enabled) {
    m_closeDeviceOnPlaybackStopItem->setChecked(enabled);
}
double AudioPage::fileBufferingSizeMsec() const {
    return m_fileBufferingSizeMsec->value();
}
void AudioPage::setFileBufferingSizeMsec(double value) {
    m_fileBufferingSizeMsec->setValue(value);
}
void AudioPage::modifyOption() {
    QSettings settings;
    settings.beginGroup("audio");
    settings.setValue("hotPlugMode", hotPlugMode());
    settings.setValue("closeDeviceAtBackground", closeDeviceAtBackground());
    settings.setValue("closeDeviceOnPlaybackStop", closeDeviceOnPlaybackStop());
    if (!qFuzzyCompare(fileBufferingSizeMsec(),
                       settings.value("fileBufferingSizeMsec", 1000.0).toDouble())) {
        settings.setValue("fileBufferingSizeMsec", fileBufferingSizeMsec());
        AudioSystem::instance()->audioContext()->handleFileBufferingSizeChange();
    }
}
void AudioPage::updateDeviceComboBox() {
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
void AudioPage::updateBufferSizeAndSampleRateComboBox() {
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
void AudioPage::updateDriverComboBox() {
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
            QMessageBox::warning(this, {},
                                 tr("No audio device available in driver mode %1!")
                                     .arg(AudioSystem::driverDisplayName(
                                         AudioSystem::instance()->driver()->name())));

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
            QMessageBox::warning(this, {},
                                 tr("Driver mode %1 cannot initialize!")
                                     .arg(AudioSystem::driverDisplayName(
                                         AudioSystem::instance()->driver()->name())));
        } else {
            updateDeviceComboBox();
        }
    });
}
void AudioPage::updateOptionsDisplay() {
    QSettings settings;
    settings.beginGroup("audio");
    setHotPlugMode(settings.value("hotPlugMode", AudioSystem::NotifyOnAnyChange)
                       .value<AudioSystem::HotPlugMode>());
    setCloseDeviceAtBackground(settings.value("closeDeviceAtBackground", false).toBool());
    setCloseDeviceOnPlaybackStop(settings.value("closeDeviceOnPlaybackStop", false).toBool());
    setFileBufferingSizeMsec(settings.value("fileBufferingSizeMsec", 1000.0).toDouble());
}