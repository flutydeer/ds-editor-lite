#include "AudioSettingsDialog.h"

#include <QBoxLayout>

#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/SeekBar.h"
#include "UI/Dialogs/Audio/OutputPlaybackPageWidget.h"

AudioSettingsDialog::AudioSettingsDialog(QWidget *parent) : OKCancelApplyDialog(parent) {
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowTitle(tr("Audio Settings"));

    auto mainLayout = new QVBoxLayout;

    m_widget = new OutputPlaybackPageWidget;

    mainLayout->addWidget(m_widget);

    body()->setLayout(mainLayout);

    connect(okButton(), &QPushButton::clicked, this, &AudioSettingsDialog::accept);
    connect(cancelButton(), &QPushButton::clicked, this, &AudioSettingsDialog::reject);
    connect(applyButton(), &QPushButton::clicked, this, &AudioSettingsDialog::applySetting);
    connect(this, &AudioSettingsDialog::accepted, this, &AudioSettingsDialog::applySetting);

}

void AudioSettingsDialog::applySetting() const {
    m_widget->accept();
}