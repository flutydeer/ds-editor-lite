//
// Created by fluty on 24-3-16.
//

#include "AppOptionsDialog.h"

#include "Controller/AppOptionsController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Pages/AppearancePage.h"
#include "UI/Controls/Button.h"

#include <QVBoxLayout>

AppOptionsDialog::AppOptionsDialog(QWidget *parent) : Dialog(parent) {
    auto appearancePage = new AppearancePage;

    auto btnOK = new Button(tr("OK"));
    btnOK->setPrimary(true);
    connect(btnOK, &Button::clicked, this, &AppOptionsDialog::accept);
    connect(this, &AppOptionsDialog::accepted, this, &AppOptionsDialog::apply);

    auto btnCancel = new Button(tr("Cancel"));
    connect(btnCancel, &Button::clicked, this, &AppOptionsDialog::cancel);

    auto btnApply = new Button(tr("Apply"));
    connect(btnApply, &Button::clicked, this, &AppOptionsDialog::apply);

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(btnOK);
    buttonLayout->addWidget(btnCancel);
    buttonLayout->addWidget(btnApply);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(appearancePage);
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
}
void AppOptionsDialog::apply() {
    AppOptionsController::instance()->apply();
}
void AppOptionsDialog::cancel() {
    AppOptionsController::instance()->cancel();
    reject();
}