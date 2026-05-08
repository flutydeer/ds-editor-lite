//
// Created by fluty on 26-5-8.
//

#include "DeveloperPage.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/OptionListCard.h"
#include "UI/Controls/SwitchButton.h"

#include <QVBoxLayout>

DeveloperPage::DeveloperPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

void DeveloperPage::modifyOption() {
    const auto option = appOptions->developer();
    option->enableDiagnostics = m_swEnableDiagnostics->value();
    appOptions->saveAndNotify(AppOptionsGlobal::DeveloperOptions);
}

QWidget *DeveloperPage::createContentWidget() {
    const auto widget = new QWidget();
    const auto option = appOptions->developer();

    m_swEnableDiagnostics = new SwitchButton(option->enableDiagnostics);
    connect(m_swEnableDiagnostics, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    const auto diagnosticsCard = new OptionListCard(tr("Diagnostics"));
    diagnosticsCard->addItem(tr("Enable diagnostic output"),
                             tr("Print event loop performance statistics to debug output"),
                             m_swEnableDiagnostics);

    const auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(diagnosticsCard, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
