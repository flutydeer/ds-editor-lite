#include "DeveloperPage.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include "Utils/UiLanguageManager.h"
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/Support/StringUtils.h>
#include "Utils/AppDumpDirectory.h"
#include "Utils/AppLogDirectory.h"

#include <synthrt/Core/Core/Runtime.h>
#include <synthrt/Core/Support/DisplayText.h>
#include <synthrt/SVS/SingerContrib.h>

#include <IcuWrapper/IcuWrapper.h>

#include <QMCore/qmsystem.h>
#include <QHBoxLayout>
#include <QLocale>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

namespace {
    /// Resolves a synthrt DisplayText for the UI using frontend-side ICU
    /// matching (ds-spec 2.4): the runtime keeps keys opaque and
    /// case-sensitive, candidates are matched with IcuWrapper, and the hit
    /// key (original spelling) is fetched by exact lookup. Falls back to the
    /// default text.
    QString displayText(const srt::core::DisplayText &text, const QStringList &candidates) {
        QStringList keys;
        const auto locales = text.locales();
        keys.reserve(static_cast<QStringList::size_type>(locales.size()));
        for (const auto &key : locales)
            keys.append(QString::fromStdString(key));
        for (const auto &tag : candidates) {
            const auto hit = IcuWrapper::bestMatch(tag, keys);
            if (!hit.isEmpty()) {
                if (const auto *value = text.text(hit.toStdString()))
                    return QString::fromStdString(*value);
            }
        }
        return QString::fromStdString(text.text());
    }
} // namespace

DeveloperPage::DeveloperPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

void DeveloperPage::modifyOption() {
    applyOptions();
}

bool DeveloperPage::applyOptions() {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return false;
    const auto snapshot = runtime->settings().getSettings();
    if (!snapshot)
        return false;
    const auto previous = snapshot.get().developer;
    auto settings = previous;
    settings.enableDiagnostics = m_swEnableDiagnostics->value();
    settings.showLogWindow = m_swShowLogWindow->value();
    settings.showTimelineDebugInfo = m_swShowTimelineDebugInfo->value();
    settings.showClipDebugInfo = m_swShowClipDebugInfo->value();
    settings.logTouchEvents = m_swLogTouchEvents->value();
    settings.enablePanelDetach = m_swEnablePanelDetach->value();
    settings.enableEmbeddedOptionsDialog = m_swEnableEmbeddedOptionsDialog->value();
    settings.editorRenderBackend =
        m_cbxEditorRenderBackend->currentData().toInt() ==
                static_cast<int>(DeveloperOption::EditorRenderBackend::RhiExperimental)
            ? Automation::EditorRenderBackend::RhiExperimental
            : Automation::EditorRenderBackend::Legacy;
    if (runtime->settings().updateDeveloper({}, settings))
        return true;

    const auto restoreSwitch = [](SwitchButton *control, bool value) {
        const QSignalBlocker blocker(control);
        control->setValue(value);
    };
    restoreSwitch(m_swEnableDiagnostics, previous.enableDiagnostics);
    restoreSwitch(m_swShowLogWindow, previous.showLogWindow);
    restoreSwitch(m_swShowTimelineDebugInfo, previous.showTimelineDebugInfo);
    restoreSwitch(m_swShowClipDebugInfo, previous.showClipDebugInfo);
    restoreSwitch(m_swLogTouchEvents, previous.logTouchEvents);
    restoreSwitch(m_swEnablePanelDetach, previous.enablePanelDetach);
    restoreSwitch(m_swEnableEmbeddedOptionsDialog, previous.enableEmbeddedOptionsDialog);
    const auto backend =
        previous.editorRenderBackend == Automation::EditorRenderBackend::RhiExperimental
            ? DeveloperOption::EditorRenderBackend::RhiExperimental
            : DeveloperOption::EditorRenderBackend::Legacy;
    const QSignalBlocker blocker(m_cbxEditorRenderBackend);
    m_cbxEditorRenderBackend->setCurrentIndex(
        m_cbxEditorRenderBackend->findData(static_cast<int>(backend)));
    return false;
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

    m_swLogTouchEvents = new SwitchButton(option->logTouchEvents);
    connect(m_swLogTouchEvents, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swEnablePanelDetach = new SwitchButton(option->enablePanelDetach);
    connect(m_swEnablePanelDetach, &SwitchButton::toggled, this, &DeveloperPage::modifyOption);

    m_swEnableEmbeddedOptionsDialog = new SwitchButton(option->enableEmbeddedOptionsDialog);
    m_swEnableEmbeddedOptionsDialog->setObjectName("developerEmbeddedOptionsDialog");
    connect(m_swEnableEmbeddedOptionsDialog, &SwitchButton::toggled, this, [this] {
        if (!applyOptions())
            return;
        const auto message = tr("The embedded options dialog setting will take effect after "
                                "restarting the app. Do you want to restart now?");
        const auto dialog = new RestartDialog(message, true, this);
        dialog->show();
    });

    m_cbxEditorRenderBackend = new ComboBox;
    m_cbxEditorRenderBackend->addItem(
        tr("Legacy (QGraphicsView)"),
        static_cast<int>(DeveloperOption::EditorRenderBackend::Legacy));
    m_cbxEditorRenderBackend->addItem(
        tr("Experimental (QRhiWidget)"),
        static_cast<int>(DeveloperOption::EditorRenderBackend::RhiExperimental));
    m_cbxEditorRenderBackend->setCurrentIndex(
        m_cbxEditorRenderBackend->findData(static_cast<int>(option->editorRenderBackend)));
    connect(m_cbxEditorRenderBackend, &ComboBox::currentIndexChanged, this, [this] {
        if (!applyOptions())
            return;
        const auto message =
            tr("The editor rendering backend will change after restarting the app. Do you want to "
               "restart now?");
        const auto dialog = new RestartDialog(message, true, this);
        dialog->show();
    });

    m_btnOpenConfigFolder = new Button(tr("Open Folder..."), this);
    connect(m_btnOpenConfigFolder, &Button::clicked, this,
            [=] { QM::reveal(appOptions->configPath()); });

    m_btnOpenLogFolder = new Button(tr("Open Folder..."), this);
    connect(m_btnOpenLogFolder, &Button::clicked, this,
            [] { AppLogDirectory::openLogDirectory(); });

    m_btnOpenDumpFolder = new Button(tr("Open Folder..."), this);
    connect(m_btnOpenDumpFolder, &Button::clicked, this,
            [] { AppDumpDirectory::openDumpDirectory(); });

    const auto appDataCard = new OptionListCard(tr("App Data"));
    appDataCard->addItem(tr("Config File"), m_btnOpenConfigFolder);
    appDataCard->addItem(tr("Log Folder"), m_btnOpenLogFolder);
    appDataCard->addItem(tr("Dump Folder"), m_btnOpenDumpFolder);

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
    diagnosticsCard->addItem(
        tr("Log touch events"),
        tr("Record every touch event the editor receives, with point states and gesture phase. "
           "Filter the log window by the EditorTouchController tag"),
        m_swLogTouchEvents);

    const auto experimentalCard = new OptionListCard(tr("Experimental"));
    experimentalCard->addItem(
        tr("Enable panel detach"),
        tr("Show the detach button on panel title bars to separate panels into standalone windows"),
        m_swEnablePanelDetach);
    experimentalCard->addItem(tr("Embedded options dialog"),
                              tr("Open the settings window inside the main window instead of a "
                                 "standalone dialog (experimental, applies after restart)"),
                              m_swEnableEmbeddedOptionsDialog);
    experimentalCard->addItem(tr("Editor rendering backend"),
                              tr("Applies to the track editor and piano roll after restart"),
                              m_cbxEditorRenderBackend);

    // Inference engine state
    const auto inferStateCard = new OptionsCard();
    inferStateCard->setTitle(tr("Inference Engine State"));

    auto treeView = new QTreeView();
    auto stateModel = new QStandardItemModel(treeView);
    stateModel->setHorizontalHeaderLabels({tr("Key"), tr("Value")});

    // Root node
    auto rootItem = stateModel->invisibleRootItem();

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
        const auto bcp47Candidates = languageManager ? languageManager->effectiveBcp47Candidates()
                                                     : QLocale::system().uiLanguages();

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
            const auto singerName = displayText(singer->name(), bcp47Candidates);
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
                const auto inferenceName = displayText(inference->name(), bcp47Candidates);
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
    treeView->setModel(stateModel);
    treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    treeView->setIndentation(10);
    treeView->expandAll();
    treeView->resizeColumnToContents(0);

    const auto inferStateLayout = new QHBoxLayout();
    inferStateLayout->setContentsMargins(10, 10, 10, 10);
    inferStateLayout->addWidget(treeView, 1);
    inferStateCard->card()->setLayout(inferStateLayout);
    inferStateCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    inferStateCard->setMinimumHeight(500);

    const auto mainLayout = new QVBoxLayout();
    // No Qt::AlignTop on any item: an alignment would pin the item to its
    // size hint inside its cell, so the surplus height of the page (its
    // viewport is usually taller than the cards' content) would be left
    // unpainted below the last card. Leaving the items unaligned lets the
    // stretch item (the inference state card) grow into that space, so the
    // page always ends flush with the last card.
    mainLayout->addWidget(appDataCard, 0);
    mainLayout->addWidget(diagnosticsCard, 0);
    mainLayout->addWidget(experimentalCard, 0);
    mainLayout->addWidget(inferStateCard, 1);
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
