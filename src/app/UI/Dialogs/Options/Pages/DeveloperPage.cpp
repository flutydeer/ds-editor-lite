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
    option->setEditorCanvasBackend(
        editorCanvasBackendFromKey(m_cbEditorRenderer->currentData().toString()));
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

    m_cbEditorRenderer = new ComboBox;
    m_cbEditorRenderer->addItem(tr("Legacy (QGraphicsView)"),
                                editorCanvasBackendKey(EditorCanvasBackend::Legacy));
    m_cbEditorRenderer->addItem(tr("Experimental QRhi (D3D11)"),
                                editorCanvasBackendKey(EditorCanvasBackend::ExperimentalRhi));
    m_cbEditorRenderer->setCurrentIndex(
        m_cbEditorRenderer->findData(editorCanvasBackendKey(option->editorCanvasBackend())));
    connect(m_cbEditorRenderer, &ComboBox::currentIndexChanged, this, [this](const int index) {
        const auto requestedBackend =
            editorCanvasBackendFromKey(m_cbEditorRenderer->itemData(index).toString());
        if (requestedBackend == appOptions->developer()->editorCanvasBackend())
            return;
        modifyOption();
        const auto message =
            tr("The editor renderer is selected during startup. The change will take effect "
               "after restarting the app. Do you want to restart now?");
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
    experimentalCard->addItem(
        tr("Enable panel detach"),
        tr("Show the detach button on panel title bars to separate panels into standalone windows"),
        m_swEnablePanelDetach);
    experimentalCard->addItem(
        tr("Editor renderer"),
        tr("Select the editor canvas implementation. Legacy is the default; QRhi uses D3D11 "
           "and falls back to Legacy if initialization fails."),
        m_cbEditorRenderer);

    const auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(diagnosticsCard, 0, Qt::AlignTop);
    mainLayout->addWidget(experimentalCard, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
