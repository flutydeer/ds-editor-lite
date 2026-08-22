#include "InferencePage.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Utils/DmlGpuUtils.h"
#include "Modules/Inference/Utils/CudaGpuUtils.h"
#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/OptionsCardItem.h>
#include <lite/GUI/Controls/SeekBarSpinboxGroup.h>
#include <lite/GUI/Controls/DoubleSeekBarSpinboxGroup.h>
#include <lite/GUI/Controls/SvsSeekbar.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/GUI/Controls/Toast.h>
#include <lite/GUI/Controls/WheelEventPolicy.h>
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include "Utils/UiLanguageManager.h"
#include <lite/Support/StringUtils.h>

#include <synthrt/Core/Core/Runtime.h>
#include <synthrt/SVS/SingerContrib.h>

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <qtconcurrentrun.h>
#include <QMCore/qmsystem.h>

#include <algorithm>

enum CustomRole {
    GpuInfoRole = Qt::UserRole,
    IsDefaultGpuRole = Qt::UserRole + 1,
};

InferencePage::InferencePage(QWidget *parent)
    : IOptionPage(parent), m_gpuDetectionWatcher(new QFutureWatcher<QList<GpuInfo>>(this)),
      m_cacheScanWatcher(new QFutureWatcher<InferCacheUtils::CacheStats>(this)) {
    connect(m_gpuDetectionWatcher, &QFutureWatcher<QList<GpuInfo>>::finished, this, [this] {
        const auto detectedProvider = m_activeGpuProvider;
        m_activeGpuProvider.clear();

        if (detectedProvider == m_requestedGpuProvider &&
            detectedProvider == m_cbExecutionProvider->currentText()) {
            applyGpuList(m_gpuDetectionWatcher->result());
        }

        if (m_requestedGpuProvider != QStringLiteral("CPU") &&
            m_requestedGpuProvider != detectedProvider) {
            startGpuDetection(m_requestedGpuProvider);
        }
    });
    connect(m_cacheScanWatcher, &QFutureWatcher<InferCacheUtils::CacheStats>::finished, this,
            [this] {
                m_btnScanCache->setEnabled(true);
                applyCacheScanResult(m_cacheScanWatcher->result());
            });
    initializePage();
}

void InferencePage::requestGpuDetection() {
    const auto provider = m_cbExecutionProvider->currentText();
    m_requestedGpuProvider = provider;

    const bool needsGpu = provider != QStringLiteral("CPU");
    m_deviceCard->setItemVisible(m_gpuItem, needsGpu);
    if (!needsGpu) {
        return;
    }

    showGpuDetectionPending();
    if (m_activeGpuProvider.isEmpty()) {
        startGpuDetection(provider);
    }
}

void InferencePage::startGpuDetection(const QString &provider) {
    m_activeGpuProvider = provider;
    m_gpuDetectionWatcher->setFuture(QtConcurrent::run([provider] {
        if (provider == QStringLiteral("DirectML")) {
            return DmlGpuUtils::getGpuList();
        }
        if (provider == QStringLiteral("CUDA")) {
            return CudaGpuUtils::getGpuList();
        }
        return QList<GpuInfo>{};
    }));
}

void InferencePage::showGpuDetectionPending() {
    const QSignalBlocker blocker(m_cbDeviceList);
    m_cbDeviceList->clear();
    m_cbDeviceList->addItem(tr("Detecting..."));
    m_cbDeviceList->setEnabled(false);
    m_gpuItem->setDescription(
        tr("GPUs with less than %L1 GiB VRAM are hidden")
            .arg(static_cast<double>(kMinGpuVramBytes) / (1024 * 1024 * 1024), 0, 'f', 0));
}

void InferencePage::applyGpuList(const QList<GpuInfo> &deviceList) {
    const QSignalBlocker blocker(m_cbDeviceList);
    m_cbDeviceList->clear();
    m_cbDeviceList->addItem(tr("Default"));
    m_cbDeviceList->setItemData(0, QVariant::fromValue<GpuInfo>({-1}), GpuInfoRole);
    m_cbDeviceList->setItemData(0, true, IsDefaultGpuRole);

    const auto option = appOptions->inference();
    int selectedIndex = 0;
    for (const auto &device : deviceList) {
        if (device.memory < kMinGpuVramBytes) {
            continue;
        }

        const int currentIndex = m_cbDeviceList->count();
        const auto displayText =
            QStringLiteral("%1 (%L2 GiB)")
                .arg(device.description)
                .arg(static_cast<double>(device.memory) / (1024 * 1024 * 1024), 0, 'f', 2);
        m_cbDeviceList->addItem(displayText);
        m_cbDeviceList->setItemData(currentIndex, QVariant::fromValue(device), GpuInfoRole);
        m_cbDeviceList->setItemData(currentIndex, false, IsDefaultGpuRole);
        if (!option->selectedGpuId.isEmpty() && device.deviceId == option->selectedGpuId) {
            selectedIndex = currentIndex;
        }
    }

    if (m_cbDeviceList->count() == 1) {
        m_cbDeviceList->clear();
        m_cbDeviceList->addItem(tr("No available GPU found"));
        m_cbDeviceList->setEnabled(false);
        m_gpuItem->setDescription(
            tr("No available GPU found. Please switch the Execution Provider above to CPU."));
        return;
    }

    m_cbDeviceList->setCurrentIndex(selectedIndex);
    m_cbDeviceList->setEnabled(true);
    m_gpuItem->setDescription(
        tr("GPUs with less than %L1 GiB VRAM are hidden")
            .arg(static_cast<double>(kMinGpuVramBytes) / (1024 * 1024 * 1024), 0, 'f', 0));
}

void InferencePage::startCacheScan() {
    m_lblCacheStats->setText(tr("Scanning..."));
    m_btnScanCache->setEnabled(false);
    m_cacheScanWatcher->setFuture(
        QtConcurrent::run(InferCacheUtils::scanCache, appOptions->inference()->cacheDirectory));
}

void InferencePage::applyCacheScanResult(const InferCacheUtils::CacheStats &stats) {
    m_lastCacheStats = stats;
    if (stats.files.isEmpty()) {
        m_lblCacheStats->setText(tr("No cache files"));
        m_btnCleanCache->setEnabled(false);
        return;
    }
    m_lblCacheStats->setText(tr("%L1 files, %2")
                                 .arg(stats.files.size())
                                 .arg(QLocale().formattedDataSize(stats.totalBytes)));
    m_btnCleanCache->setEnabled(true);
}

void InferencePage::confirmCleanCache() {
    // 主线程收集活跃集合（访问 appModel + 登记集合），再启动后台清理
    const auto active = InferCacheUtils::collectActiveCacheFiles();
    const auto cacheDir = appOptions->inference()->cacheDirectory;
    const auto deletableCount = std::count_if(
        m_lastCacheStats.files.cbegin(), m_lastCacheStats.files.cend(), [&](const auto &info) {
            const auto path = QDir(cacheDir).filePath(info.fileName);
            return !active.contains(QFileInfo(path).absoluteFilePath().toLower());
        });

    auto *dlg =
        new MessageDialog(tr("Clean Up Cache"),
                          tr("This will delete %L1 cache file(s) not used by the current project. "
                             "Files used by undo history and current playback will be kept.")
                              .arg(deletableCount),
                          this);
    dlg->addButton(tr("Cancel"), 0);
    dlg->addAccentButton(tr("Clean Up"), 1);
    if (dlg->exec() != 1)
        return;

    m_lblCacheStats->setText(tr("Cleaning..."));
    m_btnCleanCache->setEnabled(false);
    auto *watcher = new QFutureWatcher<InferCacheUtils::CleanResult>(this);
    connect(watcher, &QFutureWatcher<InferCacheUtils::CleanResult>::finished, this,
            [this, watcher] {
                const auto result = watcher->result();
                watcher->deleteLater();
                Toast::show(tr("Cache cleaned: %1 files, %2 released")
                                .arg(result.deletedCount)
                                .arg(QLocale().formattedDataSize(result.deletedBytes)));
                startCacheScan(); // 刷新统计
            });
    watcher->setFuture(QtConcurrent::run(InferCacheUtils::cleanCache, cacheDir, active));
}

void InferencePage::modifyOption() {
    const auto option = appOptions->inference();

    option->executionProvider = m_cbExecutionProvider->currentText();
    if (option->executionProvider != QStringLiteral("CPU") && m_cbDeviceList->isEnabled()) {
        if (m_cbDeviceList->currentData(IsDefaultGpuRole).toBool() == true) {
            option->selectedGpuIndex = -1;
            option->selectedGpuId = {};
        } else {
            const GpuInfo &gpuInfo = m_cbDeviceList->currentData(GpuInfoRole).value<GpuInfo>();
            option->selectedGpuIndex = gpuInfo.index;
            option->selectedGpuId = gpuInfo.deviceId;
        }
    }
    option->samplingSteps = QLocale().toInt(m_cbSamplingSteps->currentText());
    option->depth = m_dsDepthSlider->spinbox->value();
    option->runVocoderOnCpu = m_swRunVocoderOnCpu->value();
    option->autoStartInfer = m_autoStartInfer->value();
    option->singerSessionCacheCapacity = m_cbSingerSessionCacheCapacity->currentData().toInt();
    option->singerSessionIdleTimeoutSeconds = m_cbSingerSessionIdleTimeout->currentData().toInt();
    appOptions->saveAndNotify(AppOptionsGlobal::Inference);
}

QWidget *InferencePage::createContentWidget() {
    const auto widget = new QWidget();
    const auto option = appOptions->inference();
    // Device - Execution Provider
    m_cbExecutionProvider = new ComboBox();
    m_cbExecutionProvider->addItems({QStringLiteral("CPU"), QStringLiteral("DirectML")});
    if (InferenceOption::cudaExecutionProviderAvailable()) {
        m_cbExecutionProvider->addItem(QStringLiteral("CUDA"));
    }
    m_cbExecutionProvider->setCurrentText(option->executionProvider);

    // Device - GPU
    m_cbDeviceList = new ComboBox();
    m_cbDeviceList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_cbDeviceList, &ComboBox::currentIndexChanged, this, &InferencePage::modifyOption);

    // Device
    m_deviceCard = new OptionListCard(tr("Device"));
    m_deviceCard->addItem(tr("Execution Provider"), tr("App needs a restart to take effect"),
                          m_cbExecutionProvider);
    m_gpuItem = m_deviceCard->addItem(tr("GPU"), QString{}, m_cbDeviceList);
    connect(m_cbExecutionProvider, &ComboBox::currentIndexChanged, this, [this] {
        requestGpuDetection();
        modifyOption();
        const auto message = tr(
            "The settings will take effect after restarting the app. Do you want to restart now?");
        const auto dlg = new RestartDialog(message, true, this);
        dlg->show();
    });
    requestGpuDetection();

    // Render - Sampling Steps
    m_cbSamplingSteps = new ComboBox();
    m_cbSamplingSteps->setEditable(true);
    // Prevent wheel-scroll over this editable combo from grabbing focus.
    m_cbSamplingSteps->setFocusPolicy(Qt::StrongFocus);
    m_cbSamplingSteps->setFixedWidth(100);
    m_cbSamplingSteps->setValidator(new QIntValidator(1, 1000));
    const QLocale numberLocale;
    m_cbSamplingSteps->addItems({numberLocale.toString(1), numberLocale.toString(5),
                                 numberLocale.toString(10), numberLocale.toString(20),
                                 numberLocale.toString(50), numberLocale.toString(100)});
    m_cbSamplingSteps->setCurrentText(numberLocale.toString(option->samplingSteps));
    connect(m_cbSamplingSteps, &ComboBox::currentTextChanged, this, &InferencePage::modifyOption);

    // Render - Depth
    constexpr double kDsDepthMin = 0.0;
    constexpr double kDsDepthMax = 1.0;
    constexpr double kDsDepthSingleStep = 0.01;

    m_dsDepthSlider =
        new DoubleSeekBarSpinboxGroup(kDsDepthMin, kDsDepthMax, kDsDepthSingleStep, option->depth);
    m_dsDepthSlider->seekbar->setFixedWidth(256);
    // Prevent accidental value changes while scrolling the settings page.
    m_dsDepthSlider->spinbox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_dsDepthSlider->spinbox->setFocusPolicy(Qt::StrongFocus);
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

    // Render - playback lookahead window (seconds)
    m_playbackWindowSlider = new SeekBarSpinboxGroup(1, 60, 1, option->playbackLookaheadSeconds);
    m_playbackWindowSlider->seekbar->setFixedWidth(256);
    // Prevent accidental value changes while scrolling the settings page.
    m_playbackWindowSlider->spinbox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_playbackWindowSlider->spinbox->setFocusPolicy(Qt::StrongFocus);
    connect(m_playbackWindowSlider, &SeekBarSpinboxGroup::valueChanged, this,
            [&](const double value) { appOptions->inference()->playbackLookaheadSeconds = value; });
    connect(m_playbackWindowSlider, &SeekBarSpinboxGroup::editFinished, this,
            &InferencePage::modifyOption);

    // Render - pitch smooth kernel size
    m_smoothSlider = new SeekBarSpinboxGroup(0, 50, 1, option->pitch_smooth_kernel_size);
    m_smoothSlider->seekbar->setFixedWidth(256);
    // Prevent accidental value changes while scrolling the settings page.
    m_smoothSlider->spinbox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_smoothSlider->spinbox->setFocusPolicy(Qt::StrongFocus);

    connect(m_smoothSlider, &SeekBarSpinboxGroup::valueChanged, this,
            [&](const double value) { appOptions->inference()->pitch_smooth_kernel_size = value; });
    connect(m_smoothSlider, &SeekBarSpinboxGroup::editFinished, this, &InferencePage::modifyOption);


    const auto renderCard = new OptionListCard(tr("Render"));
    renderCard->addItem(tr("Sampling Steps"), m_cbSamplingSteps);
    renderCard->addItem(tr("Depth"), {m_dsDepthSlider->seekbar, m_dsDepthSlider->spinbox});
    renderCard->addItem(tr("Run Vocoder on CPU"), tr("For compatibility with legacy vocoders"),
                        m_swRunVocoderOnCpu);
    renderCard->addItem(tr("Auto Start Infer"), m_autoStartInfer);
    renderCard->addItem(tr("Playback Lookahead Window"),
                        tr("Only infer pieces within the lookahead window ahead of the playhead. "
                           "Effective when Auto Start Infer is off"),
                        {m_playbackWindowSlider->seekbar, m_playbackWindowSlider->spinbox});
    renderCard->addItem(tr("Pitch Smooth Kernel Size"),
                        tr("Smooth the pitch curve with a sinusoidal kernel"),
                        {m_smoothSlider->seekbar, m_smoothSlider->spinbox});

    m_cbSingerSessionCacheCapacity = new ComboBox();
    for (int capacity = InferenceOption::kSingerSessionCacheCapacityMin;
         capacity <= InferenceOption::kSingerSessionCacheCapacityMax; ++capacity) {
        m_cbSingerSessionCacheCapacity->addItem(QLocale().toString(capacity), capacity);
    }
    m_cbSingerSessionCacheCapacity->addItem(tr("Unlimited"),
                                            InferenceOption::kSingerSessionCacheCapacityUnlimited);
    m_cbSingerSessionCacheCapacity->setCurrentIndex(
        m_cbSingerSessionCacheCapacity->findData(option->singerSessionCacheCapacity));
    connect(m_cbSingerSessionCacheCapacity, &ComboBox::currentIndexChanged, this,
            &InferencePage::modifyOption);

    m_cbSingerSessionIdleTimeout = new ComboBox();
    for (int seconds = InferenceOption::kSingerSessionIdleTimeoutMinSeconds;
         seconds <= InferenceOption::kSingerSessionIdleTimeoutMaxSeconds;
         seconds += InferenceOption::kSingerSessionIdleTimeoutStepSeconds) {
        m_cbSingerSessionIdleTimeout->addItem(tr("%L1 seconds").arg(seconds), seconds);
    }
    m_cbSingerSessionIdleTimeout->addItem(
        tr("Unlimited"), InferenceOption::kSingerSessionIdleTimeoutUnlimitedSeconds);
    m_cbSingerSessionIdleTimeout->setCurrentIndex(
        m_cbSingerSessionIdleTimeout->findData(option->singerSessionIdleTimeoutSeconds));
    connect(m_cbSingerSessionIdleTimeout, &ComboBox::currentIndexChanged, this,
            &InferencePage::modifyOption);

    const auto singerSessionCacheCard = new OptionListCard(tr("Singer Session Retention"));
    singerSessionCacheCard->addItem(tr("Capacity"),
                                    tr("Maximum number of selected singers kept ready"),
                                    m_cbSingerSessionCacheCapacity);
    singerSessionCacheCard->addItem(tr("Idle Timeout"),
                                    tr("Release an unused selected singer after this duration"),
                                    m_cbSingerSessionIdleTimeout);

    // Cache
    m_btnOpenCacheFolder = new Button(tr("Open Folder..."), this);
    connect(m_btnOpenCacheFolder, &Button::clicked, this,
            [this] { QM::reveal(appOptions->inference()->cacheDirectory); });

    m_lblCacheStats = new QLabel(tr("Scanning..."));
    m_lblCacheStats->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_btnScanCache = new Button(tr("Refresh"), this);
    connect(m_btnScanCache, &Button::clicked, this, &InferencePage::startCacheScan);

    m_btnCleanCache = new Button(tr("Clean Up..."), this);
    m_btnCleanCache->setEnabled(false);
    connect(m_btnCleanCache, &Button::clicked, this, &InferencePage::confirmCleanCache);

    const auto cacheCard = new OptionListCard(tr("Cache"));
    cacheCard->addItem(tr("Cache Directory"), m_btnOpenCacheFolder);
    cacheCard->addItem(tr("Cache Size"), {m_lblCacheStats, m_btnScanCache, m_btnCleanCache});

    startCacheScan();

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
        const auto &su = inferEngine->constRuntime();
        // TODO(Task 7): Runtime no longer exposes packagePaths()/packages().
        // Use appOptions directly; singer list will come from SynthrtEngine.
        const auto &packagePaths = appOptions->general()->packageSearchPaths;
        const auto singerCat = su.moduleCategory("singer");
        // TODO(Task 6/7): singer category may be null until packages are opened;
        // SynthrtEngine::singers() will provide singer snapshots instead.
        const auto &singers = singerCat ? singerCat->as<srt::svs::SingerCategory>()->singers()
                                        : std::vector<srt::svs::SingerSpec *>{};

        const auto languageManager = UiLanguageManager::instance();
        const auto locale =
            languageManager ? languageManager->effectiveLocale() : QLocale::system();
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
            auto itemKey = new QStandardItem('[' + QLocale().toString(packagePathIndex) + ']');
            auto itemValue = new QStandardItem(path);
            packagePathRoot->appendRow({itemKey, itemValue});
            ++packagePathIndex;
        }
        packageRoot->appendRow(packagePathRoot);

        // Loaded packages
        auto packageLoadedRoot = new QStandardItem(tr("loaded packages"));
        // TODO(Task 7): Runtime no longer exposes packages(). Loaded package
        // info will come from SynthrtEngine::singers() snapshots.
        packageRoot->appendRow(packageLoadedRoot);

        // Loaded singers
        auto singerLoadedRoot = new QStandardItem(tr("loaded singers"));
        for (const auto singer : std::as_const(singers)) {
            if (!singer) {
                continue;
            }
            const auto singerId = QString::fromUtf8(singer->id());
            // API levels are stable protocol identifiers, not localized quantities.
            const auto singerLevel = QString::number(singer->apiLevel());
            const auto singerName = QString::fromUtf8(singer->name().text(localeName));
            const auto singerArch = QString::fromUtf8(singer->className());
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
                // API levels are stable protocol identifiers, not localized quantities.
                const auto inferenceLevel = QString::number(inference->apiLevel());
                const auto inferencePath = StringUtils::path_to_qstr(inference->path());
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
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
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
    mainLayout->addWidget(m_deviceCard, 0, Qt::AlignTop);
    mainLayout->addWidget(renderCard, 0, Qt::AlignTop);
    mainLayout->addWidget(singerSessionCacheCard, 0, Qt::AlignTop);
    mainLayout->addWidget(cacheCard, 0, Qt::AlignTop);
    mainLayout->addWidget(debugCard, 1, Qt::AlignTop);
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
