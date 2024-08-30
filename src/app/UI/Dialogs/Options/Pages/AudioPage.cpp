#include "AudioPage.h"

#include <QVBoxLayout>

#include "UI/Controls/CardView.h"
#include <UI/Dialogs/Audio/OutputPlaybackPageWidget.h>

AudioPage::AudioPage(QWidget *parent) : IOptionPage(parent) {

    auto mainLayout = new QVBoxLayout;

    m_widget = new OutputPlaybackPageWidget;

    mainLayout->addWidget(m_widget);
    setLayout(mainLayout);
}

AudioPage::~AudioPage() {
    AudioPage::modifyOption();
}

void AudioPage::modifyOption() {
    m_widget->accept();
}