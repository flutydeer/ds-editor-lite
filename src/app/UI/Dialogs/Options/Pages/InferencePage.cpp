#include "InferencePage.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/Utils/DmlUtils.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include "UI/Dialogs/Base/RestartDialog.h"

#include <QVBoxLayout>

InferencePage::InferencePage(QWidget *parent) : IOptionPage(parent) {
    auto option = appOptions->inference();
    // Device - Execution Provider
    m_cbExecutionProvider = new ComboBox();
    m_cbExecutionProvider->insertItem(0, "CPU");
    m_cbExecutionProvider->insertItem(1, "DirectML");
    if (option->executionProvider == "CPU")
        m_cbExecutionProvider->setCurrentIndex(0);
    else if (option->executionProvider == "DirectML")
        m_cbExecutionProvider->setCurrentIndex(1);
    connect(m_cbExecutionProvider, &ComboBox::currentIndexChanged, this,
            &InferencePage::modifyOption);

    auto executionProviderItem = new OptionsCardItem();
    executionProviderItem->setTitle(tr("Execution Provider"));
    executionProviderItem->setDescription(tr("App needs a restart to take effect"));
    executionProviderItem->addWidget(m_cbExecutionProvider);
    connect(m_cbExecutionProvider, &ComboBox::currentIndexChanged, this, [=] {
        // modifyOption();
        auto message = tr(
            "The settings will take effect after restarting the app. Do you want to restart now?");
        auto dlg = new RestartDialog(message, true, this);
        dlg->show();
    });

    // Device - GPU
    m_cbDeviceList = new ComboBox();
    auto deviceList = DmlUtils::getDirectXGPUs();

    for (const auto &device : std::as_const(deviceList)) {
        auto displayText =
            QStringLiteral("[%1] %2 (%3 GiB)")
                .arg(device.index)
                .arg(device.description)
                .arg(static_cast<double>(device.memory) / (1024 * 1024 * 1024), 0, 'f', 2);
        m_cbDeviceList->addItem(displayText, device.index);
    }
    m_cbDeviceList->setCurrentIndex(option->selectedGpuIndex);
    connect(m_cbDeviceList, &ComboBox::currentIndexChanged, this, &InferencePage::modifyOption);
    auto deviceListItem = new OptionsCardItem();
    deviceListItem->setTitle(tr("GPU"));
    deviceListItem->addWidget(m_cbDeviceList);

    // Device
    auto deviceCardLayout = new QVBoxLayout();
    deviceCardLayout->addWidget(executionProviderItem);
    deviceCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    deviceCardLayout->addWidget(deviceListItem);
    deviceCardLayout->setContentsMargins(10, 5, 10, 5);
    deviceCardLayout->setSpacing(0);

    auto deviceCard = new OptionsCard();
    deviceCard->setTitle(tr("Device"));
    deviceCard->card()->setLayout(deviceCardLayout);

    // Render - Speedup
    m_cbSamplingSteps = new ComboBox();
    m_cbSamplingSteps->setEditable(true);
    m_cbSamplingSteps->setFixedWidth(80);
    // m_cbSamplingSteps->setStyleSheet("padding-left:0;margin-left:0"); // avoid leading spacing
    m_cbSamplingSteps->setValidator(new QIntValidator(1, 1000));
    m_cbSamplingSteps->addItems({"1", "5", "10", "20", "50", "100"});
    m_cbSamplingSteps->setCurrentText(QString::number(option->samplingSteps));
    connect(m_cbSamplingSteps, &ComboBox::currentTextChanged, this, &InferencePage::modifyOption);

    auto samplingStepsItem = new OptionsCardItem();
    samplingStepsItem->setTitle(tr("Sampling Steps"));
    samplingStepsItem->addWidget(m_cbSamplingSteps);

    // Render - Depth
    auto doubleValidator = new QDoubleValidator();
    doubleValidator->setRange(0.0, 1.0);
    m_leDsDepth = new LineEdit(QString::number(option->depth));
    m_leDsDepth->setValidator(doubleValidator);
    m_leDsDepth->setFixedWidth(80);
    connect(m_leDsDepth, &LineEdit::editingFinished, this, &InferencePage::modifyOption);

    auto dsDepthItem = new OptionsCardItem();
    dsDepthItem->setTitle(tr("Depth"));
    dsDepthItem->addWidget(m_leDsDepth);

    // Render
    auto renderCardLayout = new QVBoxLayout();
    renderCardLayout->addWidget(samplingStepsItem);
    renderCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    renderCardLayout->addWidget(dsDepthItem);
    renderCardLayout->setContentsMargins(10, 5, 10, 5);
    renderCardLayout->setSpacing(0);

    auto renderCard = new OptionsCard();
    renderCard->setTitle(tr("Render"));
    renderCard->card()->setLayout(renderCardLayout);

    // Main Layout
    auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(deviceCard);
    mainLayout->addWidget(renderCard);
    mainLayout->addSpacerItem(
        new QSpacerItem(8, 4, QSizePolicy::Expanding, QSizePolicy::Expanding));
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
}

// InferencePage::~InferencePage() {
//     InferencePage::modifyOption();
// }

void InferencePage::modifyOption() {
    auto option = appOptions->inference();
    option->executionProvider = m_cbExecutionProvider->currentText();
    option->selectedGpuIndex = m_cbDeviceList->currentIndex();
    option->samplingSteps = m_cbSamplingSteps->currentText().toInt();
    option->depth = m_leDsDepth->text().toDouble();
    appOptions->saveAndNotify();
}