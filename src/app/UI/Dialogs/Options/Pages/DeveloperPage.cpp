//
// Created by fluty on 26-5-8.
//

#include "DeveloperPage.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/SwitchButton.h>

#include <QVBoxLayout>

DeveloperPage::DeveloperPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

void DeveloperPage::modifyOption() {
    const auto option = appOptions->developer();
    option->enableDiagnostics = m_swEnableDiagnostics->value();
    option->showLogWindow = m_swShowLogWindow->value();
    option->showTimelineDebugInfo = m_swShowTimelineDebugInfo->value();
    option->showClipDebugInfo = m_swShowClipDebugInfo->value();
    option->enablePanelDetach = m_swEnablePanelDetach->value();
    option->editorRenderBackend = static_cast<DeveloperOption::EditorRenderBackend>(
        m_cbxEditorRenderBackend->currentData().toInt());
    appOptions->saveAndNotify(AppOptionsGlobal::DeveloperOptions);
}

QWidget *DeveloperPage::createContentWidget() {
    const auto widget = new QWidget();
    const auto option = appOptions->developer();

    m_swEnableDiagnostics = new SwitchButton(option->enableDiagnostics);
    connect(m_swEnableDiagnostics, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swShowLogWindow = new SwitchButton(option->showLogWindow);
    connect(m_swShowLogWindow, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swShowTimelineDebugInfo = new SwitchButton(option->showTimelineDebugInfo);
    connect(m_swShowTimelineDebugInfo, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swShowClipDebugInfo = new SwitchButton(option->showClipDebugInfo);
    connect(m_swShowClipDebugInfo, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swEnablePanelDetach = new SwitchButton(option->enablePanelDetach);
    connect(m_swEnablePanelDetach, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_cbxEditorRenderBackend = new ComboBox;
    m_cbxEditorRenderBackend->addItem(
        tr("Legacy (QGraphicsView)"), static_cast<int>(DeveloperOption::EditorRenderBackend::Legacy));
    m_cbxEditorRenderBackend->addItem(
        tr("Experimental (QRhiWidget)"),
        static_cast<int>(DeveloperOption::EditorRenderBackend::RhiExperimental));
    m_cbxEditorRenderBackend->setCurrentIndex(
        m_cbxEditorRenderBackend->findData(static_cast<int>(option->editorRenderBackend)));
    connect(m_cbxEditorRenderBackend, &ComboBox::currentIndexChanged, this, [this] {
        modifyOption();
        const auto message = tr(
            "The editor rendering backend will change after restarting the app. Do you want to "
            "restart now?");
        const auto dialog = new RestartDialog(message, true, this);
        dialog->show();
    });

    const auto diagnosticsCard = new OptionListCard(tr("Diagnostics"));
    diagnosticsCard->addItem(tr("Enable diagnostic output"),
                             tr("Print event loop performance statistics to debug output"),
                             m_swEnableDiagnostics);
    diagnosticsCard->addItem(tr("Show log window"),
                             tr("Open a standalone window that shows application logs with "
                                "level, tag and text filters"),
                             m_swShowLogWindow);
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
    experimentalCard->addItem(tr("Editor rendering backend"),
                              tr("Applies to the track editor and piano roll after restart"),
                              m_cbxEditorRenderBackend);

    const auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(diagnosticsCard, 0, Qt::AlignTop);
    mainLayout->addWidget(experimentalCard, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
