//
// Created by fluty on 24-3-16.
//

#include "AppOptionsDialog.h"

#include "Controller/AppOptionsController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Pages/AppearancePage.h"
#include "UI/Controls/Button.h"

#include <QVBoxLayout>

AppOptionsDialog::AppOptionsDialog(QWidget *parent) : OKCancelApplyDialog(parent) {
    auto appearancePage = new AppearancePage;

    connect(okButton(), &Button::clicked, this, &AppOptionsDialog::accept);
    connect(this, &AppOptionsDialog::accepted, this, &AppOptionsDialog::apply);

    connect(cancelButton(), &Button::clicked, this, &AppOptionsDialog::cancel);

    connect(applyButton(), &Button::clicked, this, &AppOptionsDialog::apply);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(appearancePage);
    mainLayout->setContentsMargins({});
    body()->setLayout(mainLayout);
}
void AppOptionsDialog::apply() {
    AppOptionsController::instance()->apply();
}
void AppOptionsDialog::cancel() {
    AppOptionsController::instance()->cancel();
    reject();
}