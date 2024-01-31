#include "AudioSettingsDialog.h"

#include <QApplication>
#include <QBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>

AudioSettingsDialog::AudioSettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    auto mainLayout = new QVBoxLayout;

    auto audioOutputGroupBox = new QGroupBox(tr("Audio Output Options"));
    auto audioOutputLayout = new QFormLayout;
    auto driverComboBox = new QComboBox;
    audioOutputLayout->addRow(tr("Audio Driver"), driverComboBox);
    auto deviceComboBox = new QComboBox;
    audioOutputLayout->addRow(tr("Audio Device"), deviceComboBox);
    auto bufferSizeComboBox = new QComboBox;
    audioOutputLayout->addRow(tr("Buffer Size"), bufferSizeComboBox);
    auto sampleRateComboBox = new QComboBox;
    audioOutputLayout->addRow(tr("Sample Rate"), sampleRateComboBox);
    m_hotPlugModeComboBox = new QComboBox;
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
    auto okButton = new QPushButton(tr("OK"));
    okButton->setDefault(true);
    buttonLayout->addWidget(okButton);
    auto cancelButton = new QPushButton(tr("Cancel"));
    buttonLayout->addWidget(cancelButton);
    auto applyButton = new QPushButton(tr("Apply"));
    buttonLayout->addWidget(applyButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

AudioSettingsDialog::HotPlugMode AudioSettingsDialog::hotPlugMode() const {
    return HotPlugMode(m_hotPlugModeComboBox->currentIndex());
}

void AudioSettingsDialog::setHotPlugMode(AudioSettingsDialog::HotPlugMode mode) {
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

