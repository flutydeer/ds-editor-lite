#include "InferencePage.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Utils/DmlGpuUtils.h"
#include "Modules/Inference/Utils/CudaGpuUtils.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/OptionListCard.h"
#include "UI/Controls/SeekBarSpinboxGroup.h"
#include "UI/Controls/DoubleSeekBarSpinboxGroup.h"
#include "UI/Controls/SvsSeekbar.h"
#include "UI/Controls/SwitchButton.h"
#include "UI/Dialogs/Base/RestartDialog.h"

#include <QPlainTextEdit>
#include <QVBoxLayout>

enum CustomRole {
    GpuInfoRole = Qt::UserRole,
    IsDefaultGpuRole = Qt::UserRole + 1,
};

InferencePage::InferencePage(QWidget *parent) : IOptionPage(parent) {
    const auto option = appOptions->inference();
    // Device - Execution Provider
    constexpr int epIndexCpu = 0;
    constexpr int epIndexDirectML = 1;
    constexpr int epIndexCuda = 2;
    m_cbExecutionProvider = new ComboBox();
    m_cbExecutionProvider->insertItem(epIndexCpu, "CPU");
    m_cbExecutionProvider->insertItem(epIndexDirectML, "DirectML");
    m_cbExecutionProvider->insertItem(epIndexCuda, "CUDA");
    if (option->executionProvider == "CPU")
        m_cbExecutionProvider->setCurrentIndex(epIndexCpu);
    else if (option->executionProvider == "DirectML")
        m_cbExecutionProvider->setCurrentIndex(epIndexDirectML);
    else if (option->executionProvider == "CUDA")
        m_cbExecutionProvider->setCurrentIndex(epIndexCuda);
    connect(m_cbExecutionProvider, &ComboBox::currentIndexChanged, this,
            &InferencePage::modifyOption);
    connect(m_cbExecutionProvider, &ComboBox::currentIndexChanged, this, [=] {
        // modifyOption();
        const auto message = tr(
            "The settings will take effect after restarting the app. Do you want to restart now?");
        const auto dlg = new RestartDialog(message, true, this);
        dlg->show();
    });

    // Device - GPU
    m_cbDeviceList = new ComboBox();
    auto deviceList = [&]() -> QList<GpuInfo> {
        if (option->executionProvider == "DirectML") {
            return DmlGpuUtils::getGpuList();
        }
        if (option->executionProvider == "CUDA") {
            return CudaGpuUtils::getGpuList();
        }
        return {};
    }();

    m_cbDeviceList->insertItem(0, tr("Default"));
    m_cbDeviceList->setItemData(0, QVariant::fromValue<GpuInfo>({-1}), GpuInfoRole);
    m_cbDeviceList->setItemData(0, true, IsDefaultGpuRole);

    bool hasChosenDevice = false;

    for (const auto &device : std::as_const(deviceList)) {
        const int currentIndex = m_cbDeviceList->count();
        auto displayText =
            QStringLiteral("%1 (%2 GiB)")
            .arg(device.description)
            .arg(static_cast<double>(device.memory) / (1024 * 1024 * 1024), 0, 'f', 2);
        m_cbDeviceList->insertItem(currentIndex, displayText);
        m_cbDeviceList->setItemData(currentIndex, QVariant::fromValue<GpuInfo>(device),
                                    GpuInfoRole);
        m_cbDeviceList->setItemData(currentIndex, false, IsDefaultGpuRole);
        if (!hasChosenDevice) {
            if (device.deviceId == appOptions->inference()->selectedGpuId) {
                m_cbDeviceList->setCurrentIndex(currentIndex);
                hasChosenDevice = true;
            }
        }
    }
    if (const auto index_ = m_cbDeviceList->findData(option->selectedGpuIndex); index_ >= 0) {
        m_cbDeviceList->setCurrentIndex(index_);
    }
    connect(m_cbDeviceList, &ComboBox::currentIndexChanged, this, &InferencePage::modifyOption);

    // Device
    const auto deviceCard = new OptionListCard(tr("Device"));
    deviceCard->addItem(tr("Execution Provider"), tr("App needs a restart to take effect"),
                        m_cbExecutionProvider);
    deviceCard->addItem(tr("GPU"), m_cbDeviceList);

    // Render - Sampling Steps
    m_cbSamplingSteps = new ComboBox();
    m_cbSamplingSteps->setEditable(true);
    m_cbSamplingSteps->setFixedWidth(100);
    // m_cbSamplingSteps->setStyleSheet("padding-left:0;margin-left:0"); // avoid leading spacing
    m_cbSamplingSteps->setValidator(new QIntValidator(1, 1000));
    m_cbSamplingSteps->addItems({"1", "5", "10", "20", "50", "100"});
    m_cbSamplingSteps->setCurrentText(QString::number(option->samplingSteps));
    connect(m_cbSamplingSteps, &ComboBox::currentTextChanged, this, &InferencePage::modifyOption);

    // Render - Depth
    constexpr double kDsDepthMin = 0.0;
    constexpr double kDsDepthMax = 1.0;
    constexpr double kDsDepthSingleStep = 0.01;

    m_dsDepthSlider = new DoubleSeekBarSpinboxGroup(kDsDepthMin, kDsDepthMax, kDsDepthSingleStep,
                                                    option->depth);
    m_dsDepthSlider->seekbar->setFixedWidth(256);
    connect(m_dsDepthSlider, &DoubleSeekBarSpinboxGroup::valueChanged, this,
            [&](const double value) {
                appOptions->inference()->depth = value;
            });
    connect(m_dsDepthSlider, &DoubleSeekBarSpinboxGroup::editFinished, this,
            &InferencePage::modifyOption);

    // Render - Run vocoder on CPU
    auto modifyAndRestart = [&] {
        modifyOption();
        const auto message = tr(
            "The settings will take effect after restarting the app. Do you want to restart now?");
        const auto dlg = new RestartDialog(message, true, this);
        dlg->show();
    };
    m_swRunVocoderOnCpu = new SwitchButton(appOptions->inference()->runVocoderOnCpu);
    connect(m_swRunVocoderOnCpu, &SwitchButton::toggled, this, modifyAndRestart);

    // Render - decayInfer
    m_autoStartInfer = new SwitchButton(appOptions->inference()->autoStartInfer);
    connect(m_autoStartInfer, &SwitchButton::toggled, this, &InferencePage::modifyOption);

    // Render - pitch smooth kernel size
    m_smoothSlider = new SeekBarSpinboxGroup(0, 50, 1, option->pitch_smooth_kernel_size);
    m_smoothSlider->seekbar->setFixedWidth(256);

    connect(m_smoothSlider, &SeekBarSpinboxGroup::valueChanged, this, [&](const double value) {
        appOptions->inference()->pitch_smooth_kernel_size = value;
    });
    connect(m_smoothSlider, &SeekBarSpinboxGroup::editFinished, this, &InferencePage::modifyOption);


    const auto renderCard = new OptionListCard(tr("Render"));
    renderCard->addItem(tr("Sampling Steps"), m_cbSamplingSteps);
    renderCard->addItem(tr("Depth"), {m_dsDepthSlider->seekbar, m_dsDepthSlider->spinbox});
    renderCard->addItem(tr("Run Vocoder on CPU"), tr("For compatibility with legacy vocoders"),
                        m_swRunVocoderOnCpu);
    renderCard->addItem(tr("Auto Start Infer"), m_autoStartInfer);
    renderCard->addItem(tr("Pitch Smooth Kernel Size"),
                        tr("Smooth the pitch curve with a sinusoidal kernel"),
                        {m_smoothSlider->seekbar, m_smoothSlider->spinbox});

    // Debug
    m_textEdit = new QPlainTextEdit();
    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QPlainTextEdit::LineWrapMode::WidgetWidth);
    if (inferEngine) {
        auto engineInitialized = inferEngine->initialized();
        auto driverPath = inferEngine->inferenceDriverPath();
        if (driverPath.isEmpty()) {
            driverPath = "<empty>";
        }
        auto interpreterPath = inferEngine->inferenceInterpreterPath();
        if (interpreterPath.isEmpty()) {
            interpreterPath = "<empty>";
        }
        auto singerProviderPath = inferEngine->singerProviderPath();
        if (singerProviderPath.isEmpty()) {
            singerProviderPath = "<empty>";
        }
        auto runtimePath = inferEngine->inferenceRuntimePath();
        if (runtimePath.isEmpty()) {
            runtimePath = "<empty>";
        }
        QString text = QString("Engine initialized: ") +
            (engineInitialized ? "Yes" : "No") + "\n\n"
            "[Inference driver path]\n" + driverPath + "\n\n" +
            "[Inference interpreter path]\n" + interpreterPath + "\n\n" +
            "[Inference runtime path]\n" + runtimePath + "\n\n" +
            "[Singer provider path]\n" + singerProviderPath;
        m_textEdit->setPlainText(text);
    } else {
        m_textEdit->setPlainText("InferEngine is not created (null pointer)");
    }
    const auto debugCard = new OptionsCard();
    const auto debugLayout = new QHBoxLayout();
    debugLayout->setContentsMargins(10, 10, 10, 10);
    debugLayout->addWidget(m_textEdit);
    debugCard->card()->setLayout(debugLayout);
    debugCard->setTitle(tr("Debug"));

    // Main Layout
    const auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(deviceCard);
    mainLayout->addWidget(renderCard);
    mainLayout->addWidget(debugCard);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});
    setLayout(mainLayout);
}

// InferencePage::~InferencePage() {
//     InferencePage::modifyOption();
// }

void InferencePage::modifyOption() {
    const auto option = appOptions->inference();

    option->executionProvider = m_cbExecutionProvider->currentText();
    if (m_cbDeviceList->currentData(IsDefaultGpuRole).toBool() == true) {
        option->selectedGpuIndex = -1;
        option->selectedGpuId = {};
    } else {
        const GpuInfo &gpuInfo = m_cbDeviceList->currentData(GpuInfoRole).value<GpuInfo>();
        option->selectedGpuIndex = gpuInfo.index;
        option->selectedGpuId = gpuInfo.deviceId;
    }
    option->samplingSteps = m_cbSamplingSteps->currentText().toInt();
    option->depth = m_dsDepthSlider->spinbox->value();
    option->runVocoderOnCpu = m_swRunVocoderOnCpu->value();
    option->autoStartInfer = m_autoStartInfer->value();
    appOptions->saveAndNotify(AppOptionsGlobal::Inference);
}