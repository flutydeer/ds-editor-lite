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
    option->showTimelineDebugInfo = m_swShowTimelineDebugInfo->value();
    option->showClipDebugInfo = m_swShowClipDebugInfo->value();
    option->enablePanelDetach = m_swEnablePanelDetach->value();
    appOptions->saveAndNotify(AppOptionsGlobal::DeveloperOptions);
}

QWidget *DeveloperPage::createContentWidget() {
    const auto widget = new QWidget();
    const auto option = appOptions->developer();

    m_swEnableDiagnostics = new SwitchButton(option->enableDiagnostics);
    connect(m_swEnableDiagnostics, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swShowTimelineDebugInfo = new SwitchButton(option->showTimelineDebugInfo);
    connect(m_swShowTimelineDebugInfo, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swShowClipDebugInfo = new SwitchButton(option->showClipDebugInfo);
    connect(m_swShowClipDebugInfo, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swEnablePanelDetach = new SwitchButton(option->enablePanelDetach);
    connect(m_swEnablePanelDetach, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    const auto diagnosticsCard = new OptionListCard(tr("Diagnostics"));
    diagnosticsCard->addItem(tr("Enable diagnostic output"),
                             tr("Print event loop performance statistics to debug output"),
                             m_swEnableDiagnostics);
    diagnosticsCard->addItem(tr("Show timeline debug overlay"),
                             tr("Display piece boundaries and range overlays on the timeline"),
                             m_swShowTimelineDebugInfo);
    diagnosticsCard->addItem(tr("Show clip debug info"),
                             tr("Display clip ID and detailed time info on track clips"),
                             m_swShowClipDebugInfo);

    const auto experimentalCard = new OptionListCard(tr("Experimental"));
    experimentalCard->addItem(tr("Enable panel detach"),
                              tr("Show the detach button on panel title bars to separate panels into standalone windows"),
                              m_swEnablePanelDetach);

    const auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(diagnosticsCard, 0, Qt::AlignTop);
    mainLayout->addWidget(experimentalCard, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
