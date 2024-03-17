//
// Created by fluty on 24-3-18.
//

#include "GeneralPage.h"

#include <QLabel>
#include <QVBoxLayout>

GeneralPage::GeneralPage(QWidget *parent) : IOptionPage(parent) {
    auto label = new QLabel("General Page");
    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label);
    setLayout(mainLayout);
}
void GeneralPage::modifyOption() {
}