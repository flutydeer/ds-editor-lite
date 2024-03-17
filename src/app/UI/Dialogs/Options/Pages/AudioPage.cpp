//
// Created by fluty on 24-3-18.
//

#include "AudioPage.h"

#include <QLabel>
#include <QVBoxLayout>

AudioPage::AudioPage(QWidget *parent) : IOptionPage(parent) {
    auto label = new QLabel("Audio Page");
    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label);
    setLayout(mainLayout);
}
void AudioPage::modifyOption() {
}