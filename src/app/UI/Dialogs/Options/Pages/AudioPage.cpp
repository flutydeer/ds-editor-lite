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
#include "UI/Controls/SwitchButton.h"

#include <UI/Dialogs/Audio/OutputPlaybackPageWidget.h>

#include <TalcsDevice/AudioDriverManager.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsRemote/RemoteAudioDevice.h>
#include <Model/AppOptions/Options/AudioOption.h>

AudioPage::AudioPage(QWidget *parent) : IOptionPage(parent) {

    auto mainLayout = new QVBoxLayout;

    m_widget = new OutputPlaybackPageWidget;

    mainLayout->addWidget(m_widget);
    setLayout(mainLayout);
}

void AudioPage::modifyOption() {
    m_widget->accept();
}