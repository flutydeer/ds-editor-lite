#include "DeveloperPage.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/SwitchButton.h>

#include <QSignalBlocker>
#include <QVBoxLayout>

DeveloperPage::DeveloperPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
    connect(appOptions, &AppOptions::optionsChanged, this,
            [this](const AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::DeveloperOptions ||
                    option == AppOptionsGlobal::All) {
                    syncFromOptions();
                }
            });
}

void DeveloperPage::modifyOption() {
    const auto option = appOptions->developer();
    option->enableDiagnostics = m_swEnableDiagnostics->value();
    option->showLogWindow = m_swShowLogWindow->value();
    option->showTimelineDebugInfo = m_swShowTimelineDebugInfo->value();
    option->showClipDebugInfo = m_swShowClipDebugInfo->value();
    option->enablePanelDetach = m_swEnablePanelDetach->value();
    option->enableEmbeddedOptionsDialog = m_swEnableEmbeddedOptionsDialog->value();
    option->editorRenderBackend = static_cast<DeveloperOption::EditorRenderBackend>(
        m_cbxEditorRenderBackend->currentData().toInt());
    appOptions->saveAndNotify(AppOptionsGlobal::DeveloperOptions);
}

void DeveloperPage::syncFromOptions() {
    const auto option = appOptions->developer();
    const QSignalBlocker diagnosticsBlocker(m_swEnableDiagnostics);
    const QSignalBlocker logWindowBlocker(m_swShowLogWindow);
    const QSignalBlocker timelineDebugInfoBlocker(m_swShowTimelineDebugInfo);
    const QSignalBlocker clipDebugInfoBlocker(m_swShowClipDebugInfo);
    const QSignalBlocker panelDetachBlocker(m_swEnablePanelDetach);
    const QSignalBlocker embeddedOptionsDialogBlocker(m_swEnableEmbeddedOptionsDialog);
    const QSignalBlocker renderBackendBlocker(m_cbxEditorRenderBackend);

    m_swEnableDiagnostics->setValue(option->enableDiagnostics);
    m_swShowLogWindow->setValue(option->showLogWindow);
    m_swShowTimelineDebugInfo->setValue(option->showTimelineDebugInfo);
    m_swShowClipDebugInfo->setValue(option->showClipDebugInfo);
    m_swEnablePanelDetach->setValue(option->enablePanelDetach);
    m_swEnableEmbeddedOptionsDialog->setValue(option->enableEmbeddedOptionsDialog);
    m_cbxEditorRenderBackend->setCurrentIndex(
        m_cbxEditorRenderBackend->findData(static_cast<int>(option->editorRenderBackend)));
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

    m_swEnableEmbeddedOptionsDialog = new SwitchButton(option->enableEmbeddedOptionsDialog);
    connect(m_swEnableEmbeddedOptionsDialog, &SwitchButton::toggled, this, [this] {
        modifyOption();
        const auto message = tr("The embedded options dialog setting will take effect after "
                                "restarting the app. Do you want to restart now?");
        const auto dialog = new RestartDialog(message, true, this);
        dialog->show();
    });

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
    experimentalCard->addItem(tr("Embedded options dialog"),
                              tr("Open the settings window inside the main window instead of a standalone dialog (experimental, applies after restart)"),
                              m_swEnableEmbeddedOptionsDialog);
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
