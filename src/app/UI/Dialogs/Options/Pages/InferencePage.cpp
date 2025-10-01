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
#include "Utils/StringUtils.h"

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/SVS/SingerContrib.h>

#include <QDir>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

enum CustomRole {
    GpuInfoRole = Qt::UserRole,
    IsDefaultGpuRole = Qt::UserRole + 1,
};

InferencePage::InferencePage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
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

QWidget *InferencePage::createContentWidget() {
    const auto widget = new QWidget();
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
    connect(m_cbExecutionProvider, &ComboBox::currentIndexChanged, this, [this] {
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

    m_dsDepthSlider =
        new DoubleSeekBarSpinboxGroup(kDsDepthMin, kDsDepthMax, kDsDepthSingleStep, option->depth);
    m_dsDepthSlider->seekbar->setFixedWidth(256);
    connect(m_dsDepthSlider, &DoubleSeekBarSpinboxGroup::valueChanged, this,
            [&](const double value) { appOptions->inference()->depth = value; });
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

    connect(m_smoothSlider, &SeekBarSpinboxGroup::valueChanged, this,
            [&](const double value) { appOptions->inference()->pitch_smooth_kernel_size = value; });
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
    m_treeView = new QTreeView();
    auto debugModel = new QStandardItemModel();
    debugModel->setHorizontalHeaderLabels({tr("Key"), tr("Value")});

    // Root node
    auto rootItem = debugModel->invisibleRootItem();

    if (inferEngine) {
        const auto fillEmpty = [](QString str_) {
            if (str_.isEmpty()) {
                return QString("<empty>");
            }
            return str_;
        };
        const auto &su = inferEngine->constSynthUnit();
        const auto packagePaths = su.packagePaths();
        const auto packages = su.packages();
        const auto &singerCategory = *su.category("singer")->as<srt::SingerCategory>();
        const auto &singers = singerCategory.singers();

        const auto locale = QLocale::system();
        const auto localeName = locale.name().toStdString();

        const auto engineInitialized = inferEngine->initialized();
        const auto driverPath = fillEmpty(inferEngine->inferenceDriverPath());
        const auto interpreterPath = fillEmpty(inferEngine->inferenceInterpreterPath());
        const auto singerProviderPath = fillEmpty(inferEngine->singerProviderPath());
        const auto runtimePath = fillEmpty(inferEngine->inferenceRuntimePath());
        const auto configPath = fillEmpty(inferEngine->configPath());

        const QString kStringYes = tr("Yes");
        const QString kStringNo = tr("No");

        // Engine root node
        auto engineRoot = new QStandardItem(tr("engine"));
        rootItem->appendRow(engineRoot);

        // Engine initialized
        auto engineItem = new QStandardItem(tr("initialized"));
        auto engineValue = new QStandardItem(engineInitialized ? kStringYes : kStringNo);
        engineRoot->appendRow({engineItem, engineValue});

        auto enginePathRoot = new QStandardItem(tr("plugins"));

        // Inference driver path
        auto driverItem = new QStandardItem("inference driver");
        auto driverValue = new QStandardItem(driverPath);
        enginePathRoot->appendRow({driverItem, driverValue});

        // Inference interpreter path
        auto interpreterItem = new QStandardItem("inference interpreter");
        auto interpreterValue = new QStandardItem(interpreterPath);
        enginePathRoot->appendRow({interpreterItem, interpreterValue});

        // Inference runtime path
        auto runtimeItem = new QStandardItem("inference runtime");
        auto runtimeValue = new QStandardItem(runtimePath);
        enginePathRoot->appendRow({runtimeItem, runtimeValue});

        // Singer provider path
        auto singerItem = new QStandardItem("singer provider");
        auto singerValue = new QStandardItem(singerProviderPath);
        enginePathRoot->appendRow({singerItem, singerValue});

        engineRoot->appendRow(enginePathRoot);

        // Package root node
        auto packageRoot = new QStandardItem(tr("package"));
        rootItem->appendRow(packageRoot);

        // Package path
        auto packagePathRoot = new QStandardItem(tr("search paths"));
        int packagePathIndex = 0;
        for (const auto &path : std::as_const(packagePaths)) {
            auto itemKey = new QStandardItem('[' + QString::number(packagePathIndex) + ']');
            auto itemValue = new QStandardItem(StringUtils::path_to_qstr(path));
            packagePathRoot->appendRow({itemKey, itemValue});
            ++packagePathIndex;
        }
        packageRoot->appendRow(packagePathRoot);

        // Loaded packages
        auto packageLoadedRoot = new QStandardItem(tr("loaded packages"));
        for (const auto &pkg : std::as_const(packages)) {
            const auto pkgId = QString::fromUtf8(pkg.id());
            const auto pkgVersion = QString::fromUtf8(pkg.version().toString());
            const auto pkgVendor = QString::fromUtf8(pkg.vendor().text(localeName));
            const auto pkgPath = StringUtils::path_to_qstr(pkg.path());
            auto currentPackageRoot = new QStandardItem(pkgId + " (" + pkgVersion + ')');
            currentPackageRoot->appendRow({
                new QStandardItem(tr("id")),
                new QStandardItem(pkgId),
            });
            currentPackageRoot->appendRow({
                new QStandardItem(tr("version")),
                new QStandardItem(pkgVersion),
            });
            currentPackageRoot->appendRow({
                new QStandardItem(tr("vendor")),
                new QStandardItem(pkgVendor),
            });
            currentPackageRoot->appendRow(
                {new QStandardItem(tr("path")), new QStandardItem(pkgPath)});
            packageLoadedRoot->appendRow(currentPackageRoot);
        }
        packageRoot->appendRow(packageLoadedRoot);

        // Loaded singers
        auto singerLoadedRoot = new QStandardItem(tr("loaded singers"));
        for (const auto singer : std::as_const(singers)) {
            if (!singer) {
                continue;
            }
            const auto singerId = QString::fromUtf8(singer->id());
            const auto singerLevel = QString::number(singer->apiLevel());
            const auto singerName = QString::fromUtf8(singer->name().text(localeName));
            const auto singerArch = QString::fromUtf8(singer->arch());
            const auto singerPath = StringUtils::path_to_qstr(singer->path());
            const auto singerImports = singer->imports();

            auto currentSingerRoot = new QStandardItem(singerName + " (" + singerId + ')');
            currentSingerRoot->appendRow({
                new QStandardItem(tr("id")),
                new QStandardItem(singerId),
            });
            currentSingerRoot->appendRow({
                new QStandardItem(tr("name")),
                new QStandardItem(singerName),
            });
            currentSingerRoot->appendRow({
                new QStandardItem(tr("api level")),
                new QStandardItem(singerLevel),
            });
            currentSingerRoot->appendRow({
                new QStandardItem(tr("architecture")),
                new QStandardItem(singerArch),
            });
            currentSingerRoot->appendRow({
                new QStandardItem(tr("path")),
                new QStandardItem(singerPath),
            });
            auto inferenceRoot = new QStandardItem(tr("inferences"));
            for (const auto &singerImport : std::as_const(singerImports)) {
                const auto inference = singerImport.inference();
                if (!inference) {
                    continue;
                }
                const auto inferenceName = QString::fromUtf8(inference->name().text(localeName));
                const auto inferenceClassName = QString::fromUtf8(inference->className());
                const auto inferenceLevel = QString::number(inference->apiLevel());
                const auto inferencePath = StringUtils::path_to_qstr(inference->path());
                const auto inferenceLocator = singerImport.inferenceLocator();
                const auto inferenceLocatorStr = QString::fromUtf8(inferenceLocator.toString());
                auto currentInferenceRoot = new QStandardItem(inferenceName);
                currentInferenceRoot->appendRow({
                    new QStandardItem(tr("name")),
                    new QStandardItem(inferenceName),
                });
                currentInferenceRoot->appendRow({
                    new QStandardItem(tr("class name")),
                    new QStandardItem(inferenceClassName),
                });
                currentInferenceRoot->appendRow({
                    new QStandardItem(tr("api level")),
                    new QStandardItem(inferenceLevel),
                });
                currentInferenceRoot->appendRow({
                    new QStandardItem(tr("path")),
                    new QStandardItem(inferencePath),
                });
                currentInferenceRoot->appendRow({
                    new QStandardItem(tr("locator")),
                    new QStandardItem(inferenceLocatorStr),
                });
                inferenceRoot->appendRow(currentInferenceRoot);
            }
            currentSingerRoot->appendRow(inferenceRoot);
            singerLoadedRoot->appendRow(currentSingerRoot);
        }
        packageRoot->appendRow(singerLoadedRoot);
    } else {
        // Engine root node
        auto engineRoot = new QStandardItem(tr("engine"));
        rootItem->appendRow(engineRoot);
        // Engine initialized
        auto engineItem = new QStandardItem(tr("initialized"));
        auto engineValue = new QStandardItem(tr("InferEngine is not created (null pointer)"));
        engineRoot->appendRow({engineItem, engineValue});
    }
    m_treeView->setModel(debugModel);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setIndentation(10);
    m_treeView->expandAll();
    m_treeView->resizeColumnToContents(0);
    const auto debugCard = new OptionsCard();
    const auto debugLayout = new QHBoxLayout();
    debugLayout->setContentsMargins(10, 10, 10, 10);
    debugLayout->addWidget(m_treeView, 1);
    debugCard->card()->setLayout(debugLayout);
    debugCard->setTitle(tr("Debug"));
    debugCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    debugCard->setMinimumHeight(500);

    // Main Layout
    const auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(deviceCard, 0, Qt::AlignTop);
    mainLayout->addWidget(renderCard, 0, Qt::AlignTop);
    mainLayout->addWidget(debugCard, 1, Qt::AlignTop);
    // mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}