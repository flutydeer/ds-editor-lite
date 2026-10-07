#include "InferencePage.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/ExecutionProvider.h"
#include "Modules/Inference/Utils/DmlGpuUtils.h"
#include "Modules/Inference/Utils/CudaGpuUtils.h"
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

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <qtconcurrentrun.h>
#include <QMCore/qmsystem.h>

#include <algorithm>
#include <utility>

enum CustomRole {
    GpuInfoRole = Qt::UserRole,
    IsDefaultGpuRole = Qt::UserRole + 1,
};

InferencePage::InferencePage(QWidget *parent, GpuDetector gpuDetector)
    : IOptionPage(parent), m_gpuDetectionWatcher(new QFutureWatcher<QList<GpuInfo>>(this)),
      m_gpuDetector(std::move(gpuDetector)),
      m_cacheScanWatcher(new QFutureWatcher<InferCacheUtils::CacheStats>(this)) {
    if (!m_gpuDetector) {
        m_gpuDetector = [](const QString &provider) {
            if (provider == QStringLiteral("DirectML"))
                return DmlGpuUtils::getGpuList();
            if (provider == QStringLiteral("CUDA"))
                return CudaGpuUtils::getGpuList();
            return QList<GpuInfo>{};
        };
    }
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
    m_gpuDetectionWatcher->setFuture(QtConcurrent::run(m_gpuDetector, provider));
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
            tr("No available GPU found. The execution provider has been switched back to CPU."));

        // A GPU provider cannot run without a usable device, so keeping it in
        // appConfig.json only reproduces the startup failure on every launch.
        const auto cpuProvider = ExecutionProviderUtils::toString(ExecutionProvider::Cpu);
        if (m_cbExecutionProvider->currentText() != cpuProvider) {
            // Block the combo signal on purpose: the user did not ask for this
            // change, so no restart prompt here -- the corrected value applies on
            // the next launch. The Toast below explains what happened.
            const QSignalBlocker blocker(m_cbExecutionProvider);
            m_cbExecutionProvider->setCurrentText(cpuProvider);
            modifyOption();
            Toast::show(tr("No available GPU found. The execution provider has been switched "
                           "back to CPU."));
        }
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
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    const auto snapshot = runtime->settings().getSettings();
    if (!snapshot)
        return;
    auto settings = snapshot.get().inference;
    settings.executionProvider = m_cbExecutionProvider->currentText();
    if (settings.executionProvider != QStringLiteral("CPU") && m_cbDeviceList->isEnabled()) {
        if (m_cbDeviceList->currentData(IsDefaultGpuRole).toBool() == true) {
            settings.selectedGpuIndex = -1;
            settings.selectedGpuId = {};
        } else {
            const GpuInfo &gpuInfo = m_cbDeviceList->currentData(GpuInfoRole).value<GpuInfo>();
            settings.selectedGpuIndex = gpuInfo.index;
            settings.selectedGpuId = gpuInfo.deviceId;
        }
    }
    settings.samplingSteps = QLocale().toInt(m_cbSamplingSteps->currentText());
    settings.depth = m_dsDepthSlider->spinbox->value();
    settings.runVocoderOnCpu = m_swRunVocoderOnCpu->value();
    settings.autoStartInference = m_autoStartInfer->value();
    settings.playbackLookaheadSeconds = m_playbackWindowSlider->spinbox->value();
    settings.pitchSmoothKernelSize = m_smoothSlider->spinbox->value();
    settings.singerSessionCacheCapacity = m_cbSingerSessionCacheCapacity->currentData().toInt();
    settings.singerSessionIdleTimeoutSeconds = m_cbSingerSessionIdleTimeout->currentData().toInt();
    runtime->settings().updateInference({}, settings);
}

QWidget *InferencePage::createContentWidget() {
    const auto widget = new QWidget();
    const auto option = appOptions->inference();
    // Device - Execution Provider
    m_cbExecutionProvider = new ComboBox();
    m_cbExecutionProvider->setObjectName(QStringLiteral("inferenceExecutionProvider"));
    for (const auto provider :
         {ExecutionProvider::Cpu, ExecutionProvider::DirectML, ExecutionProvider::Cuda}) {
        if (ExecutionProviderUtils::availableInBuild(provider))
            m_cbExecutionProvider->addItem(ExecutionProviderUtils::toString(provider));
    }
    m_cbExecutionProvider->setCurrentText(option->executionProvider);

    // Device - GPU
    m_cbDeviceList = new ComboBox();
    m_cbDeviceList->setObjectName(QStringLiteral("inferenceDevice"));
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
    m_cbSamplingSteps->setObjectName(QStringLiteral("inferenceSamplingSteps"));
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
    m_swRunVocoderOnCpu->setObjectName(QStringLiteral("inferenceRunVocoderOnCpu"));
    connect(m_swRunVocoderOnCpu, &SwitchButton::toggled, this, modifyAndRestart);

    // Render - decayInfer
    m_autoStartInfer = new SwitchButton(appOptions->inference()->autoStartInfer);
    m_autoStartInfer->setObjectName(QStringLiteral("inferenceAutoStart"));
    connect(m_autoStartInfer, &SwitchButton::toggled, this, &InferencePage::modifyOption);

    // Render - playback lookahead window (seconds)
    m_playbackWindowSlider = new SeekBarSpinboxGroup(1, 60, 1, option->playbackLookaheadSeconds);
    m_playbackWindowSlider->seekbar->setFixedWidth(256);
    // Prevent accidental value changes while scrolling the settings page.
    m_playbackWindowSlider->spinbox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_playbackWindowSlider->spinbox->setFocusPolicy(Qt::StrongFocus);
    connect(m_playbackWindowSlider, &SeekBarSpinboxGroup::editFinished, this,
            &InferencePage::modifyOption);

    // Render - pitch smooth kernel size
    m_smoothSlider = new SeekBarSpinboxGroup(0, 50, 1, option->pitch_smooth_kernel_size);
    m_smoothSlider->seekbar->setFixedWidth(256);
    // Prevent accidental value changes while scrolling the settings page.
    m_smoothSlider->spinbox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_smoothSlider->spinbox->setFocusPolicy(Qt::StrongFocus);

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
    m_lblCacheStats->setObjectName(QStringLiteral("inferenceCacheStats"));
    m_lblCacheStats->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_btnScanCache = new Button(tr("Refresh"), this);
    m_btnScanCache->setObjectName(QStringLiteral("inferenceScanCache"));
    connect(m_btnScanCache, &Button::clicked, this, &InferencePage::startCacheScan);

    m_btnCleanCache = new Button(tr("Clean Up..."), this);
    m_btnCleanCache->setObjectName(QStringLiteral("inferenceCleanCache"));
    m_btnCleanCache->setEnabled(false);
    connect(m_btnCleanCache, &Button::clicked, this, &InferencePage::confirmCleanCache);

    const auto cacheCard = new OptionListCard(tr("Cache"));
    cacheCard->addItem(tr("Cache Directory"), m_btnOpenCacheFolder);
    cacheCard->addItem(tr("Cache Size"), {m_lblCacheStats, m_btnScanCache, m_btnCleanCache});

    startCacheScan();

    // Main Layout
    const auto mainLayout = new QVBoxLayout();
    // No Qt::AlignTop: an alignment pins each card to its size hint inside its
    // cell, so the surplus height of a tall viewport is spread into the cells
    // and shows up as inflated gaps between the cards. The trailing stretch
    // takes that space instead, keeping the 12px card spacing (same structure
    // as the other option pages).
    mainLayout->addWidget(m_deviceCard);
    mainLayout->addWidget(renderCard);
    mainLayout->addWidget(singerSessionCacheCard);
    mainLayout->addWidget(cacheCard);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
