#include "InferencePage.h"

#include "Modules/Inference/DmlUtils.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include "UI/Dialogs/Base/RestartDialog.h"

#include <QVBoxLayout>

InferencePage::InferencePage(QWidget *parent) : IOptionPage(parent) {
    // Device - Execution Provider
    m_cbExecutionProvider = new ComboBox();
    m_cbExecutionProvider->insertItem(0, "CPU");
    m_cbExecutionProvider->insertItem(1, "DirectML");
    m_cbExecutionProvider->setCurrentIndex(1);

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
    auto deviceListItem = new OptionsCardItem();
    deviceListItem->setTitle("GPU");
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
    m_cbDsSpeedup = new ComboBox();
    m_cbDsSpeedup->setEditable(true);
    m_cbDsSpeedup->setFixedWidth(80);
    m_cbDsSpeedup->setStyleSheet("padding-left:0;margin-left:0"); // avoid leading spacing
    m_cbDsSpeedup->setValidator(new QIntValidator(1, 1000));
    m_cbDsSpeedup->addItems({"1", "5", "10", "20", "50", "100"});
    m_cbDsSpeedup->setCurrentText("20"); // TODO: retrieve actual speedup

    auto dsSpeedupItem = new OptionsCardItem();
    dsSpeedupItem->setTitle("Speedup");
    dsSpeedupItem->addWidget(m_cbDsSpeedup);

    // Render - Depth
    auto doubleValidator = new QDoubleValidator();
    doubleValidator->setRange(0.0, 1.0);
    m_leDsDepth = new LineEdit("1.0"); // TODO: retrieve actual depth
    m_leDsDepth->setValidator(doubleValidator);
    m_leDsDepth->setFixedWidth(80);

    auto dsDepthItem = new OptionsCardItem();
    dsDepthItem->setTitle("Depth");
    dsDepthItem->addWidget(m_leDsDepth);

    // Render
    auto renderCardLayout = new QVBoxLayout();
    renderCardLayout->addWidget(dsSpeedupItem);
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

InferencePage::~InferencePage() {
    InferencePage::modifyOption();
}

void InferencePage::modifyOption() {
}