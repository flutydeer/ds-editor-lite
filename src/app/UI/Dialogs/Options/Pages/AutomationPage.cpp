#include "AutomationPage.h"

#include "Automation/Mcp/McpClientConfiguration.h"
#include "Automation/EditorAutomationRuntimeStatus.h"
#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/ProductMetadata.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/OptionsCardItem.h>
#include <lite/GUI/Controls/PathEditor.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/GUI/Controls/ToolButton.h>
#include <lite/GUI/Controls/Toast.h>
#include <lite/GUI/Controls/WheelEventPolicy.h>

#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

AutomationPage::AutomationPage(QWidget *parent) : IOptionPage(parent) {
    if (auto *application = QCoreApplication::instance())
        application->installEventFilter(this);
    initializePage();
}

void AutomationPage::setCustomPermissionOperationIds(QStringList operationIds) {
    operationIds.removeAll(QString{});
    operationIds.removeDuplicates();
    if (operationIds == m_customPermissionOperationIds)
        return;

    m_customPermissionOperationIds = std::move(operationIds);
    initializePage();
}

QString AutomationPage::settingDescription(const QString &description,
                                           const StartupArguments::ConfigSource source) const {
    if (source == StartupArguments::ConfigSource::Persisted)
        return description;
    const auto overrideNote = tr("Overridden and locked by a command-line argument");
    return description.isEmpty() ? overrideNote : tr("%1 (%2)").arg(description, overrideNote);
}

QString AutomationPage::categoryDisplayName(const QString &category) const {
    if (category == QStringLiteral("application"))
        return tr("Application");
    if (category == QStringLiteral("automation"))
        return tr("Automation");
    if (category == QStringLiteral("documents"))
        return tr("Documents");
    if (category == QStringLiteral("project"))
        return tr("Project");
    if (category == QStringLiteral("notes"))
        return tr("Notes");
    if (category == QStringLiteral("parameters"))
        return tr("Parameters");
    if (category == QStringLiteral("timeline"))
        return tr("Timeline");
    if (category == QStringLiteral("history"))
        return tr("History");
    if (category == QStringLiteral("voices"))
        return tr("Voices");
    if (category == QStringLiteral("tracks"))
        return tr("Tracks");
    if (category == QStringLiteral("bus"))
        return tr("Bus");
    if (category == QStringLiteral("clips"))
        return tr("Clips");
    if (category == QStringLiteral("speaker_mix"))
        return tr("Speaker Mix");
    if (category == QStringLiteral("tempos"))
        return tr("Tempos");
    if (category == QStringLiteral("time_signatures"))
        return tr("Time Signatures");
    if (category == QStringLiteral("master"))
        return tr("Master");
    if (category == QStringLiteral("formats"))
        return tr("Formats");
    if (category == QStringLiteral("audio_clips"))
        return tr("Audio Clips");
    if (category == QStringLiteral("exports"))
        return tr("Exports");
    if (category == QStringLiteral("extract"))
        return tr("Extraction");
    if (category == QStringLiteral("inference"))
        return tr("Inference");
    if (category == QStringLiteral("tasks"))
        return tr("Tasks");
    if (category == QStringLiteral("playback"))
        return tr("Playback");
    if (category == QStringLiteral("workspace"))
        return tr("Workspace");
    if (category == QStringLiteral("track_panel"))
        return tr("Track Panel");
    if (category == QStringLiteral("clip_editor"))
        return tr("Clip Editor");
    if (category == QStringLiteral("settings"))
        return tr("Settings");
    if (category == QStringLiteral("packages"))
        return tr("Packages");
    if (category == QStringLiteral("lyric_rules"))
        return tr("Lyric Rules");
    return category;
}

QString AutomationPage::runtimeStateDescription(const QString &state) const {
    if (state == QStringLiteral("editor_starting"))
        return tr("Editor starting");
    if (state == QStringLiteral("server_disabled"))
        return tr("Server disabled");
    if (state == QStringLiteral("server_starting"))
        return tr("Server starting");
    if (state == QStringLiteral("server_ready"))
        return tr("Server ready");
    if (state == QStringLiteral("server_stopping"))
        return tr("Server stopping");
    if (state == QStringLiteral("editor_stopping"))
        return tr("Editor stopping");
    if (state == QStringLiteral("error"))
        return tr("Error");
    return state;
}

bool AutomationPage::eventFilter(QObject *watched, QEvent *event) {
    if (watched == QCoreApplication::instance() && event->type() == QEvent::DynamicPropertyChange)
        refreshRuntimeStatus();
    return IOptionPage::eventFilter(watched, event);
}

void AutomationPage::refreshCategoryPermissionSwitches() {
    for (auto it = m_customCategorySwitches.constBegin(); it != m_customCategorySwitches.constEnd();
         ++it) {
        const auto operationIds = m_customCategoryOperationIds.value(it.key());
        const auto enabledCount = std::count_if(
            operationIds.cbegin(), operationIds.cend(), [this](const QString &operationId) {
                const auto operation = m_customPermissionSwitches.value(operationId);
                return operation && operation->value();
            });
        const QSignalBlocker blocker(it.value());
        it.value()->setValue(enabledCount > 0);
        if (auto *header = m_customCategoryHeaderItems.value(it.key())) {
            header->setTitle(tr("%1 (%L2/%L3 enabled)")
                                 .arg(categoryDisplayName(it.key()))
                                 .arg(enabledCount)
                                 .arg(operationIds.size()));
        }
    }
    refreshCustomToolsetSummary();
}

void AutomationPage::refreshCustomToolsetSummary() {
    if (!m_customToolsetItem)
        return;

    const auto *option = appOptions->automation();
    const auto enabledCount = std::count_if(
        m_customPermissionOperationIds.cbegin(), m_customPermissionOperationIds.cend(),
        [this, option](const QString &operationId) {
            if (const auto *permissionSwitch = m_customPermissionSwitches.value(operationId))
                return permissionSwitch->value();
            return option->customPermissionEnabled(operationId);
        });
    m_customToolsetItem->setDescription(
        tr("%L1/%L2 enabled").arg(enabledCount).arg(m_customPermissionOperationIds.size()));
}

QString AutomationPage::currentMcpEndpoint() const {
    QString endpoint;
    if (const auto *application = QCoreApplication::instance()) {
        endpoint =
            application->property(Automation::AutomationRuntimeStatus::EndpointProperty).toString();
    }
    if (endpoint.isEmpty() && m_controlPort) {
        const auto port =
            m_effectiveConfig.controlPortSource == StartupArguments::ConfigSource::CommandLine
                ? m_effectiveConfig.controlPort
                : static_cast<quint16>(m_controlPort->value());
        endpoint = QStringLiteral("http://127.0.0.1:%1/mcp").arg(port);
    }
    return endpoint;
}

void AutomationPage::refreshRuntimeStatus() {
    const auto *application = QCoreApplication::instance();
    if (!application)
        return;
    const auto state =
        application->property(Automation::AutomationRuntimeStatus::StateProperty).toString();
    const auto error =
        application->property(Automation::AutomationRuntimeStatus::ErrorProperty).toString();
    if (m_runtimeStateValue)
        m_runtimeStateValue->setText(state.isEmpty() ? tr("Not initialized")
                                                     : runtimeStateDescription(state));
    if (m_runtimeErrorValue)
        m_runtimeErrorValue->setText(error);
    if (m_serverCard && m_runtimeErrorItem)
        m_serverCard->setItemVisible(m_runtimeErrorItem, !error.isEmpty());
}

void AutomationPage::modifyOption() {
    auto *option = appOptions->automation();
    if (m_effectiveConfig.mcpEnabledSource == StartupArguments::ConfigSource::Persisted)
        option->mcpEnabled = m_mcpEnabled->value();
    if (m_effectiveConfig.controlPortSource == StartupArguments::ConfigSource::Persisted)
        option->controlPort = static_cast<quint16>(m_controlPort->value());
    if (m_effectiveConfig.controlLevelSource == StartupArguments::ConfigSource::Persisted) {
        option->controlLevel =
            static_cast<AutomationOption::ControlLevel>(m_controlLevel->currentData().toInt());
    }

    const auto canonicalRoots = [](const QStringList &paths, QStringList &invalid) {
        QStringList result;
        for (const auto &path : paths) {
            const QFileInfo info(path.trimmed());
            const auto canonical = info.isDir() ? info.canonicalFilePath() : QString{};
            if (canonical.isEmpty()) {
                if (!path.trimmed().isEmpty())
                    invalid.append(path.trimmed());
                continue;
            }
            result.append(canonical);
        }
        result.removeDuplicates();
        return result;
    };
    QStringList invalidAccessRoots;
    const auto accessRoots = canonicalRoots(m_accessRoots->paths(), invalidAccessRoots);
    if (invalidAccessRoots.isEmpty()) {
        option->accessRoots = accessRoots;
        const QSignalBlocker blocker(m_accessRoots);
        m_accessRoots->setPaths(option->accessRoots);
    }
    if (m_accessRootsItem) {
        m_accessRootsItem->setDescription(
            invalidAccessRoots.isEmpty()
                ? tr("Limits MCP file access, such as opening, importing, saving, and exporting, "
                     "to the listed folders")
                : tr("Not saved because a folder is missing or invalid: %1")
                      .arg(invalidAccessRoots.join(QStringLiteral(", "))));
    }
    for (auto it = m_customPermissionSwitches.constBegin();
         it != m_customPermissionSwitches.constEnd(); ++it) {
        option->setCustomPermissionEnabled(it.key(), it.value()->value());
    }
    appOptions->saveAndNotify(AppOptionsGlobal::Automation);
    refreshRuntimeStatus();
}

QWidget *AutomationPage::createContentWidget() {
    const auto pageHost = new QWidget;
    m_pageHostLayout = new QVBoxLayout(pageHost);
    m_pageHostLayout->setContentsMargins({});
    m_pageHostLayout->setSpacing(0);
    m_accessControlPage = new QWidget;
    m_toolsetPage = nullptr;
    m_serverCard = nullptr;
    m_runtimeStateValue = nullptr;
    m_runtimeErrorValue = nullptr;
    m_runtimeErrorItem = nullptr;
    m_customToolsetItem = nullptr;
    auto *option = appOptions->automation();
    const auto parsedArguments = StartupArguments::parseApplicationArguments();
    m_effectiveConfig = StartupArguments::effectiveAutomationConfig(
        *option, parsedArguments.isValid() ? parsedArguments.automation
                                           : StartupArguments::AutomationOverrides{});

    m_mcpEnabled = new SwitchButton(m_effectiveConfig.mcpEnabled);
    m_mcpEnabled->setEnabled(m_effectiveConfig.mcpEnabledSource ==
                             StartupArguments::ConfigSource::Persisted);
    connect(m_mcpEnabled, &SwitchButton::toggled, this, &AutomationPage::modifyOption);

    m_randomizeControlPort = new Button(tr("Randomize"));
    m_controlPort = new SVS::ExpressionSpinBox;
    m_controlPort->setRange(1, 65535);
    m_controlPort->setValue(m_effectiveConfig.controlPort);
    m_controlPort->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_controlPort->setFocusPolicy(Qt::StrongFocus);
    const auto controlPortEditable =
        m_effectiveConfig.controlPortSource == StartupArguments::ConfigSource::Persisted;
    m_randomizeControlPort->setEnabled(controlPortEditable);
    m_controlPort->setEnabled(controlPortEditable);
    connect(m_randomizeControlPort, &Button::clicked, this, [this] {
        const QSignalBlocker blocker(m_controlPort);
        m_controlPort->setValue(
            AutomationOption::generateRandomControlPort(m_controlPort->value()));
        modifyOption();
    });
    connect(m_controlPort, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &AutomationPage::modifyOption);

    m_controlLevel = new ComboBox;
    m_controlLevel->addItem(tr("L1 - Basic Editing"),
                            static_cast<int>(AutomationOption::ControlLevel::L1));
    m_controlLevel->addItem(tr("L2 - Complete Creation"),
                            static_cast<int>(AutomationOption::ControlLevel::L2));
    m_controlLevel->addItem(tr("L3 - Advanced Control"),
                            static_cast<int>(AutomationOption::ControlLevel::L3));
    m_controlLevel->addItem(tr("Custom"), static_cast<int>(AutomationOption::ControlLevel::Custom));
    m_controlLevel->setCurrentIndex(
        m_controlLevel->findData(static_cast<int>(m_effectiveConfig.controlLevel)));
    m_controlLevel->setEnabled(m_effectiveConfig.controlLevelSource ==
                               StartupArguments::ConfigSource::Persisted);
    connect(m_controlLevel, &ComboBox::currentIndexChanged, this, &AutomationPage::modifyOption);

    m_serverCard = new OptionListCard(tr("Local Server"));
    m_serverCard->addItem(tr("Enable MCP Server"),
                          settingDescription({}, m_effectiveConfig.mcpEnabledSource), m_mcpEnabled);
    auto *controlPortControl = new QWidget;
    auto *controlPortLayout = new QHBoxLayout(controlPortControl);
    controlPortLayout->setContentsMargins({});
    controlPortLayout->setSpacing(6);
    controlPortLayout->addWidget(m_randomizeControlPort);
    controlPortLayout->addWidget(m_controlPort);
    m_serverCard->addItem(tr("Control Port"),
                          settingDescription({}, m_effectiveConfig.controlPortSource),
                          controlPortControl);
    const auto createRuntimeValue = [controlPortControl] {
        auto *value = new QLabel;
        value->setObjectName(QStringLiteral("desc"));
        value->setMinimumHeight(controlPortControl->sizeHint().height());
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return value;
    };
    m_runtimeStateValue = createRuntimeValue();
    m_runtimeErrorValue = createRuntimeValue();
    m_serverCard->addItem(tr("Runtime Status"), m_runtimeStateValue);
    m_runtimeErrorItem = m_serverCard->addItem(tr("Last Error"), m_runtimeErrorValue);
    refreshRuntimeStatus();

    const auto connectionCard = new OptionListCard(tr("Connection Configurations"));
    const auto currentStdioConfiguration = [this] {
        const auto controlLevel =
            static_cast<AutomationOption::ControlLevel>(m_controlLevel->currentData().toInt());
        QStringList enabledCustomOperations;
        if (controlLevel == AutomationOption::ControlLevel::Custom) {
            const auto *option = appOptions->automation();
            for (const auto &operationId : std::as_const(m_customPermissionOperationIds)) {
                if (option->customPermissionEnabled(operationId))
                    enabledCustomOperations.append(operationId);
            }
        }
        const auto command = Automation::McpClientConfiguration::connectorExecutablePath(
            QCoreApplication::applicationDirPath());
        const auto arguments = Automation::McpClientConfiguration::connectorArguments(
            controlLevel, enabledCustomOperations);
        return Automation::McpClientConfiguration::stdioJson(command, arguments);
    };
    auto *stdioCopyButton = new Button(tr("Copy Configuration"));
    stdioCopyButton->setObjectName(QStringLiteral("automationStdioConfigurationCopyButton"));
    connect(stdioCopyButton, &Button::clicked, this, [currentStdioConfiguration] {
        QGuiApplication::clipboard()->setText(currentStdioConfiguration());
        Toast::show(tr("STDIO configuration copied"));
    });
    connectionCard->addItem(
        tr("STDIO Connector"),
        tr("Starts %1 and discovers this editor automatically")
            .arg(QString::fromLatin1(LiteProductMetadata::ConnectorProductName)),
        stdioCopyButton);

    auto *streamableHttpCopyButton = new Button(tr("Copy Configuration"));
    streamableHttpCopyButton->setObjectName(
        QStringLiteral("automationStreamableHttpConfigurationCopyButton"));
    connect(streamableHttpCopyButton, &Button::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(
            Automation::McpClientConfiguration::streamableHttpJson(currentMcpEndpoint()));
        Toast::show(tr("Streamable HTTP configuration copied"));
    });
    auto *streamableHttpEndpointCopyButton = new Button(tr("Copy Endpoint"));
    streamableHttpEndpointCopyButton->setObjectName(
        QStringLiteral("automationStreamableHttpEndpointCopyButton"));
    connect(streamableHttpEndpointCopyButton, &Button::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(currentMcpEndpoint());
        Toast::show(tr("Endpoint copied"));
    });
    connectionCard->addItem(
        tr("Streamable HTTP"), tr("Connects directly to the editor's configured MCP endpoint"),
        QList<QWidget *>{streamableHttpEndpointCopyButton, streamableHttpCopyButton});

    m_customPermissionSwitches.clear();
    m_customCategorySwitches.clear();
    m_customCategoryHeaderItems.clear();
    m_customCategoryOperationIds.clear();
    auto *importControlLevelButton = new Button(tr("Import Control Level"));
    importControlLevelButton->setObjectName(QStringLiteral("automationImportControlLevelButton"));
    const auto refreshImportControlLevelButton = [this, importControlLevelButton] {
        const auto controlLevel =
            static_cast<AutomationOption::ControlLevel>(m_controlLevel->currentData().toInt());
        importControlLevelButton->setEnabled(controlLevel !=
                                             AutomationOption::ControlLevel::Custom);
    };
    refreshImportControlLevelButton();
    connect(m_controlLevel, &ComboBox::currentIndexChanged, importControlLevelButton,
            refreshImportControlLevelButton);
    connect(importControlLevelButton, &Button::clicked, this, &AutomationPage::importControlLevel);

    auto *openToolsetButton = new ToolButton;
    openToolsetButton->setObjectName(QStringLiteral("automationOpenToolsetButton"));
    openToolsetButton->setFixedSize(28, 28);
    openToolsetButton->setActionIcon(QStringLiteral(":/svg/icons/chevron_right_16_regular.svg"));
    openToolsetButton->setToolTip(tr("Open toolset"));
    connect(openToolsetButton, &QPushButton::clicked, this, &AutomationPage::showToolsetPage);

    m_accessRoots = new PathEditor;
    m_accessRoots->setPaths(option->accessRoots);
    connect(m_accessRoots, &PathEditor::pathsChanged, this, &AutomationPage::modifyOption);
    const auto accessControlCard = new OptionListCard;
    accessControlCard->setTitleVisible(false);
    accessControlCard->addItem(tr("Control Level"),
                               settingDescription(tr("Determines which tools clients can access"),
                                                  m_effectiveConfig.controlLevelSource),
                               m_controlLevel);
    m_customToolsetItem =
        accessControlCard->addItem(tr("Custom Toolset"), QString{},
                                   QList<QWidget *>{importControlLevelButton, openToolsetButton});
    refreshCustomToolsetSummary();
    m_accessRootsItem = accessControlCard->addItem(
        tr("File Access Permissions"),
        tr("Limits MCP file access, such as opening, importing, saving, and exporting, to the "
           "listed folders"),
        m_accessRoots);
    m_accessRootsItem->setTextAreaAlignment(Qt::AlignTop);

    const auto accessControlSection = new QWidget;
    const auto accessControlLayout = new QVBoxLayout(accessControlSection);
    accessControlLayout->setContentsMargins({});
    accessControlLayout->setSpacing(6);
    const auto accessControlTitle = new QLabel(tr("Access Control"), accessControlSection);
    accessControlTitle->setObjectName(QStringLiteral("automationAccessControlSectionTitle"));
    accessControlTitle->setProperty("optionsSectionTitle", true);
    accessControlTitle->setContentsMargins(10, 0, 0, 0);
    accessControlLayout->addWidget(accessControlTitle);
    accessControlLayout->addWidget(accessControlCard);

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_serverCard, 0, Qt::AlignTop);
    mainLayout->addWidget(connectionCard, 0, Qt::AlignTop);
    mainLayout->addWidget(accessControlSection, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(12);
    m_accessControlPage->setLayout(mainLayout);
    m_accessControlPage->setContentsMargins({});
    m_pageHostLayout->addWidget(m_accessControlPage);

    if (m_toolsetPageVisible) {
        ensureToolsetPage();
        m_accessControlPage->hide();
        m_toolsetPage->show();
    }
    return pageHost;
}

QWidget *AutomationPage::createToolsetPage() {
    const auto page = new QWidget;
    const auto layout = new QVBoxLayout(page);
    layout->setContentsMargins({});
    layout->setSpacing(6);

    auto *backButton = new ToolButton(page);
    backButton->setObjectName(QStringLiteral("automationCloseToolsetButton"));
    backButton->setFixedSize(28, 28);
    backButton->setActionIcon(QStringLiteral(":/svg/icons/chevron_left_16_regular.svg"));
    backButton->setToolTip(tr("Back to Access Control"));
    connect(backButton, &QPushButton::clicked, this, &AutomationPage::showAccessControlPage);

    const auto title = new QLabel(tr("Custom Toolset"), page);
    title->setObjectName(QStringLiteral("automationToolsetPageTitle"));
    title->setProperty("optionsSectionTitle", true);

    const auto header = new QWidget(page);
    const auto headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins({});
    headerLayout->setSpacing(6);
    headerLayout->addWidget(backButton);
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    layout->addWidget(header);

    QStringList categoryOrder;
    for (const auto &operationId : std::as_const(m_customPermissionOperationIds)) {
        if (const auto *contract = AutomationWire::findPublicTool(operationId);
            contract && !contract->category.isEmpty()) {
            if (!m_customCategoryOperationIds.contains(contract->category))
                categoryOrder.append(contract->category);
            m_customCategoryOperationIds[contract->category].append(operationId);
        }
    }

    if (m_customPermissionOperationIds.isEmpty()) {
        const auto emptyCard = new OptionListCard;
        emptyCard->setTitleVisible(false);
        emptyCard->addItem(tr("No public tools available"),
                           tr("Public tools appear here when the automation manifest is ready"));
        layout->addWidget(emptyCard);
    } else {
        const auto *option = appOptions->automation();
        for (const auto &category : std::as_const(categoryOrder)) {
            const auto operationIds = m_customCategoryOperationIds.value(category);
            auto *categoryCard = new OptionListCard;
            categoryCard->setTitleVisible(false);
            categoryCard->setObjectName(
                QStringLiteral("automationCustomToolGroup_%1").arg(category));

            auto *expandButton = new ToolButton(categoryCard);
            expandButton->setObjectName(
                QStringLiteral("automationCustomToolGroupExpand_%1").arg(category));
            expandButton->setFixedSize(28, 28);
            expandButton->setCheckable(true);
            expandButton->setActionIcon(QStringLiteral(":/svg/icons/chevron_right_16_regular.svg"));
            expandButton->setToolTip(tr("Expand tool group"));

            const auto anyEnabled = std::any_of(
                operationIds.cbegin(), operationIds.cend(), [option](const QString &operationId) {
                    return option->customPermissionEnabled(operationId);
                });
            auto *categorySwitch = new SwitchButton(anyEnabled, categoryCard);
            categorySwitch->setObjectName(
                QStringLiteral("automationCustomToolGroupSwitch_%1").arg(category));
            categorySwitch->setToolTip(tr("Enable or disable all tools in this group"));
            m_customCategorySwitches.insert(category, categorySwitch);

            auto *headerItem = categoryCard->addItem(QString{}, {expandButton, categorySwitch});
            m_customCategoryHeaderItems.insert(category, headerItem);

            QList<OptionsCardItem *> operationItems;
            for (const auto &operationId : operationIds) {
                auto *permissionSwitch =
                    new SwitchButton(option->customPermissionEnabled(operationId));
                permissionSwitch->setObjectName(
                    QStringLiteral("automationCustomTool_%1").arg(operationId));
                m_customPermissionSwitches.insert(operationId, permissionSwitch);
                connect(permissionSwitch, &SwitchButton::toggled, this, [this] {
                    refreshCategoryPermissionSwitches();
                    modifyOption();
                });
                auto *item = categoryCard->addItem(operationId, permissionSwitch);
                categoryCard->setItemVisible(item, false);
                operationItems.append(item);
            }

            connect(expandButton, &QPushButton::toggled, this,
                    [this, categoryCard, expandButton, operationItems](const bool expanded) {
                        for (auto *item : operationItems)
                            categoryCard->setItemVisible(item, expanded);
                        expandButton->setActionIcon(
                            expanded ? QStringLiteral(":/svg/icons/chevron_down_16_regular.svg")
                                     : QStringLiteral(":/svg/icons/chevron_right_16_regular.svg"));
                        expandButton->setToolTip(expanded ? tr("Collapse tool group")
                                                          : tr("Expand tool group"));
                    });
            connect(categorySwitch, &SwitchButton::toggled, this,
                    [this, operationIds](const bool enabled) {
                        for (const auto &operationId : operationIds) {
                            if (auto *operation = m_customPermissionSwitches.value(operationId)) {
                                const QSignalBlocker blocker(operation);
                                operation->setValue(enabled);
                            }
                        }
                        refreshCategoryPermissionSwitches();
                        modifyOption();
                    });
            layout->addWidget(categoryCard);
        }
        refreshCategoryPermissionSwitches();
    }

    layout->addStretch();
    return page;
}

void AutomationPage::ensureToolsetPage() {
    if (m_toolsetPage || !m_pageHostLayout)
        return;
    m_toolsetPage = createToolsetPage();
    m_pageHostLayout->addWidget(m_toolsetPage);
    m_toolsetPage->hide();
}

void AutomationPage::showAccessControlPage() {
    if (!m_accessControlPage)
        return;
    m_toolsetPageVisible = false;
    if (m_toolsetPage)
        m_toolsetPage->hide();
    m_accessControlPage->show();
    verticalScrollBar()->setValue(0);
}

void AutomationPage::showToolsetPage() {
    ensureToolsetPage();
    if (!m_accessControlPage || !m_toolsetPage)
        return;
    m_toolsetPageVisible = true;
    m_accessControlPage->hide();
    m_toolsetPage->show();
    verticalScrollBar()->setValue(0);
}

void AutomationPage::importControlLevel() {
    const auto controlLevel =
        static_cast<AutomationOption::ControlLevel>(m_controlLevel->currentData().toInt());
    const auto wireControlLevel =
        AutomationWire::controlLevelFromName(AutomationOption::controlLevelToString(controlLevel));
    if (!wireControlLevel || *wireControlLevel == AutomationWire::ControlLevel::Custom)
        return;

    auto *option = appOptions->automation();
    option->customPermissions.clear();
    for (const auto &operationId : std::as_const(m_customPermissionOperationIds)) {
        const auto *contract = AutomationWire::findPublicTool(operationId);
        const auto enabled = contract && AutomationWire::presetIncludes(
                                             *wireControlLevel, contract->minimumControlLevel);
        option->setCustomPermissionEnabled(operationId, enabled);
        if (auto *permissionSwitch = m_customPermissionSwitches.value(operationId)) {
            const QSignalBlocker blocker(permissionSwitch);
            permissionSwitch->setValue(enabled);
        }
    }
    refreshCategoryPermissionSwitches();
    modifyOption();
}
