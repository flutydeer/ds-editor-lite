#include "AutomationPage.h"

#include "Automation/Mcp/McpClientConfiguration.h"
#include "Automation/Mcp/EditorMcpRuntimeStatus.h"
#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/OptionsCardItem.h>
#include <lite/GUI/Controls/PathEditor.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/GUI/Controls/ToolButton.h>
#include <lite/GUI/Controls/Toast.h>

#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

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

QString AutomationPage::sourceDescription(const StartupArguments::ConfigSource source,
                                          const QString &optionName) const {
    if (source == StartupArguments::ConfigSource::CommandLine) {
        return tr("Overridden for this run by %1; the saved value is unchanged").arg(optionName);
    }
    return tr("Saved in the application configuration");
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
    if (state == QStringLiteral("starting"))
        return tr("Starting");
    if (state == QStringLiteral("mcp_disabled"))
        return tr("MCP disabled");
    if (state == QStringLiteral("mcp_starting"))
        return tr("MCP starting");
    if (state == QStringLiteral("mcp_ready"))
        return tr("MCP ready");
    if (state == QStringLiteral("mcp_stopping"))
        return tr("MCP stopping");
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
}

void AutomationPage::refreshConnectionConfigurations() {
    if (m_stdioConfiguration && m_profile) {
        const auto profile =
            static_cast<AutomationOption::Profile>(m_profile->currentData().toInt());
        QStringList enabledCustomOperations;
        if (profile == AutomationOption::Profile::Custom) {
            const auto *option = appOptions->automation();
            for (const auto &operationId : std::as_const(m_customPermissionOperationIds)) {
                if (option->customPermissionEnabled(operationId))
                    enabledCustomOperations.append(operationId);
            }
        }
        const auto command = Automation::McpClientConfiguration::connectorExecutablePath(
            QCoreApplication::applicationDirPath());
        const auto arguments = Automation::McpClientConfiguration::connectorArguments(
            profile, enabledCustomOperations);
        m_stdioConfiguration->setPlainText(
            Automation::McpClientConfiguration::stdioJson(command, arguments));
    }

    if (!m_streamableHttpConfiguration)
        return;
    QString endpoint;
    if (const auto *application = QCoreApplication::instance()) {
        endpoint = application->property(Automation::McpRuntimeStatus::EndpointProperty).toString();
    }
    if (endpoint.isEmpty() && m_controlPort) {
        const auto port =
            m_effectiveConfig.controlPortSource == StartupArguments::ConfigSource::CommandLine
                ? m_effectiveConfig.controlPort
                : static_cast<quint16>(m_controlPort->value());
        endpoint = QStringLiteral("http://127.0.0.1:%1/mcp").arg(port);
    }
    m_streamableHttpConfiguration->setPlainText(
        Automation::McpClientConfiguration::streamableHttpJson(endpoint));
}

void AutomationPage::refreshRuntimeStatus() {
    const auto *application = QCoreApplication::instance();
    if (!application)
        return;
    const auto state =
        application->property(Automation::McpRuntimeStatus::StateProperty).toString();
    const auto endpoint =
        application->property(Automation::McpRuntimeStatus::EndpointProperty).toString();
    const auto error =
        application->property(Automation::McpRuntimeStatus::ErrorProperty).toString();
    if (m_runtimeStateItem)
        m_runtimeStateItem->setDescription(state.isEmpty() ? tr("Not initialized")
                                                           : runtimeStateDescription(state));
    if (m_runtimeEndpointItem) {
        m_runtimeEndpointItem->setDescription(endpoint.isEmpty() ? tr("Not listening") : endpoint);
    }
    if (m_runtimeErrorItem)
        m_runtimeErrorItem->setDescription(error.isEmpty() ? tr("No error") : error);
    refreshConnectionConfigurations();
}

void AutomationPage::modifyOption() {
    auto *option = appOptions->automation();
    if (m_effectiveConfig.mcpEnabledSource == StartupArguments::ConfigSource::Persisted)
        option->mcpEnabled = m_mcpEnabled->value();
    option->controlPort = static_cast<quint16>(m_controlPort->value());
    if (m_effectiveConfig.profileSource == StartupArguments::ConfigSource::Persisted) {
        option->selectedProfile =
            static_cast<AutomationOption::Profile>(m_profile->currentData().toInt());
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
    QStringList invalidReadRoots;
    QStringList invalidWriteRoots;
    const auto readRoots = canonicalRoots(m_readRoots->paths(), invalidReadRoots);
    const auto writeRoots = canonicalRoots(m_writeRoots->paths(), invalidWriteRoots);
    if (invalidReadRoots.isEmpty()) {
        option->readRoots = readRoots;
        const QSignalBlocker readBlocker(m_readRoots);
        m_readRoots->setPaths(option->readRoots);
    }
    if (invalidWriteRoots.isEmpty()) {
        option->writeRoots = writeRoots;
        const QSignalBlocker writeBlocker(m_writeRoots);
        m_writeRoots->setPaths(option->writeRoots);
    }
    if (m_readRootsItem) {
        m_readRootsItem->setDescription(
            invalidReadRoots.isEmpty()
                ? tr("Limits MCP file reads, such as opening and importing, to the listed folders. "
                     "Existing folders are saved as canonical paths.")
                : tr("Not saved because a folder is missing or invalid: %1")
                      .arg(invalidReadRoots.join(QStringLiteral(", "))));
    }
    if (m_writeRootsItem) {
        m_writeRootsItem->setDescription(
            invalidWriteRoots.isEmpty()
                ? tr("Limits MCP file writes, such as saving and exporting, to the listed folders. "
                     "Existing folders are saved as canonical paths.")
                : tr("Not saved because a folder is missing or invalid: %1")
                      .arg(invalidWriteRoots.join(QStringLiteral(", "))));
    }
    for (auto it = m_customPermissionSwitches.constBegin();
         it != m_customPermissionSwitches.constEnd(); ++it) {
        option->setCustomPermissionEnabled(it.key(), it.value()->value());
    }
    appOptions->saveAndNotify(AppOptionsGlobal::Automation);
    refreshConnectionConfigurations();
}

QWidget *AutomationPage::createContentWidget() {
    const auto widget = new QWidget;
    m_stdioConfiguration = nullptr;
    m_streamableHttpConfiguration = nullptr;
    auto *option = appOptions->automation();
    const auto parsedArguments = StartupArguments::parseApplicationArguments();
    m_effectiveConfig = StartupArguments::effectiveAutomationConfig(
        *option, parsedArguments.isValid() ? parsedArguments.automation
                                           : StartupArguments::AutomationOverrides{});

    m_mcpEnabled = new SwitchButton(m_effectiveConfig.mcpEnabled);
    m_mcpEnabled->setEnabled(m_effectiveConfig.mcpEnabledSource ==
                             StartupArguments::ConfigSource::Persisted);
    connect(m_mcpEnabled, &SwitchButton::toggled, this, &AutomationPage::modifyOption);

    m_refreshControlPort = new Button(tr("Refresh"));
    m_controlPort = new SVS::ExpressionSpinBox;
    m_controlPort->setRange(1, 65535);
    m_controlPort->setValue(option->controlPort);
    connect(m_refreshControlPort, &Button::clicked, this, [this] {
        const QSignalBlocker blocker(m_controlPort);
        m_controlPort->setValue(
            AutomationOption::generateRandomControlPort(m_controlPort->value()));
        modifyOption();
    });
    connect(m_controlPort, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &AutomationPage::modifyOption);

    m_profile = new ComboBox;
    m_profile->addItem(tr("L1 - Basic Editing"), static_cast<int>(AutomationOption::Profile::L1));
    m_profile->addItem(tr("L2 - Complete Creation"),
                       static_cast<int>(AutomationOption::Profile::L2));
    m_profile->addItem(tr("L3 - Advanced Control"),
                       static_cast<int>(AutomationOption::Profile::L3));
    m_profile->addItem(tr("Custom"), static_cast<int>(AutomationOption::Profile::Custom));
    m_profile->setCurrentIndex(m_profile->findData(static_cast<int>(m_effectiveConfig.profile)));
    m_profile->setEnabled(m_effectiveConfig.profileSource ==
                          StartupArguments::ConfigSource::Persisted);
    connect(m_profile, &ComboBox::currentIndexChanged, this, &AutomationPage::modifyOption);

    const auto serverCard = new OptionListCard(tr("MCP Server"));
    serverCard->addItem(
        tr("Enable MCP Server"),
        sourceDescription(m_effectiveConfig.mcpEnabledSource, m_effectiveConfig.mcpEnabled
                                                                  ? QStringLiteral("--mcp")
                                                                  : QStringLiteral("--no-mcp")),
        m_mcpEnabled);
    auto *controlPortControl = new QWidget;
    auto *controlPortLayout = new QHBoxLayout(controlPortControl);
    controlPortLayout->setContentsMargins({});
    controlPortLayout->setSpacing(6);
    controlPortLayout->addWidget(m_refreshControlPort);
    controlPortLayout->addWidget(m_controlPort);
    serverCard->addItem(
        tr("Control Port"),
        sourceDescription(m_effectiveConfig.controlPortSource, QStringLiteral("--control-port")),
        controlPortControl);
    m_runtimeStateItem = serverCard->addItem(tr("Runtime Status"), QString{});
    m_runtimeEndpointItem = serverCard->addItem(tr("Current Endpoint"), QString{});
    m_runtimeErrorItem = serverCard->addItem(tr("Last Error"), QString{});
    refreshRuntimeStatus();

    const auto connectionCard = new OptionListCard(tr("Connection Configurations"));
    const auto createConfigurationControl = [this](QPlainTextEdit *&configuration,
                                                   Button *&copyButton,
                                                   const QString &copiedMessage,
                                                   const QString &objectName,
                                                   const QString &sizingConfiguration,
                                                   const bool reserveHorizontalScrollBar) {
        auto *container = new QWidget;
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins({});
        layout->setSpacing(6);

        configuration = new QPlainTextEdit(container);
        configuration->setObjectName(objectName);
        configuration->setReadOnly(true);
        configuration->setLineWrapMode(QPlainTextEdit::NoWrap);
        configuration->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        configuration->document()->setDocumentMargin(0);
        configuration->setMinimumWidth(420);
        configuration->setPlainText(sizingConfiguration);
        configuration->ensurePolished();
        auto *document = configuration->document();
        auto *documentLayout = document->documentLayout();
        qreal contentHeight = 0;
        for (auto block = document->begin(); block.isValid(); block = block.next())
            contentHeight += documentLayout->blockBoundingRect(block).height();
        const auto viewportHeight = qCeil(contentHeight + document->documentMargin() + 1);
        const auto contentsMargins = configuration->contentsMargins();
        const auto verticalMargins = contentsMargins.top() + contentsMargins.bottom();
        configuration->horizontalScrollBar()->ensurePolished();
        const auto horizontalScrollBarHeight =
            reserveHorizontalScrollBar ? configuration->horizontalScrollBar()->sizeHint().height()
                                       : 0;
        configuration->setFixedHeight(viewportHeight + verticalMargins + horizontalScrollBarHeight);
        QTimer::singleShot(0, configuration, [configuration, viewportHeight] {
            const auto heightAdjustment = viewportHeight - configuration->viewport()->height();
            if (heightAdjustment != 0)
                configuration->setFixedHeight(configuration->height() + heightAdjustment);
        });
        layout->addWidget(configuration);

        copyButton = new Button(tr("Copy Configuration"), container);
        copyButton->setObjectName(objectName + QStringLiteral("CopyButton"));
        connect(copyButton, &Button::clicked, this, [configuration, copiedMessage] {
            QGuiApplication::clipboard()->setText(configuration->toPlainText());
            Toast::show(copiedMessage);
        });
        layout->addWidget(copyButton, 0, Qt::AlignRight);
        return container;
    };

    Button *stdioCopyButton = nullptr;
    const auto presetStdioConfiguration = Automation::McpClientConfiguration::stdioJson(
        QString{},
        Automation::McpClientConfiguration::connectorArguments(AutomationOption::Profile::L1));
    const auto stdioControl = createConfigurationControl(
        m_stdioConfiguration, stdioCopyButton, tr("STDIO configuration copied"),
        QStringLiteral("automationStdioConfiguration"), presetStdioConfiguration, true);
    connectionCard->addItem(
        tr("STDIO Connector"),
        tr("Starts DS Connector Lite and discovers this editor automatically. The copied JSON "
           "uses the current access profile."),
        stdioControl);

    Button *streamableHttpCopyButton = nullptr;
    const auto streamableHttpConfiguration =
        Automation::McpClientConfiguration::streamableHttpJson(QString{});
    const auto streamableHttpControl =
        createConfigurationControl(m_streamableHttpConfiguration, streamableHttpCopyButton,
                                   tr("Streamable HTTP configuration copied"),
                                   QStringLiteral("automationStreamableHttpConfiguration"),
                                   streamableHttpConfiguration, false);
    connectionCard->addItem(
        tr("Streamable HTTP"),
        tr("Connects directly to the editor's configured MCP endpoint. The generated port stays "
           "unchanged until you edit it or press Refresh."),
        streamableHttpControl);
    refreshConnectionConfigurations();

    const auto accessCard = new OptionListCard(tr("Access Profile"));
    accessCard->addItem(
        tr("Profile"),
        sourceDescription(m_effectiveConfig.profileSource, QStringLiteral("--automation-profile")),
        m_profile);

    m_customPermissionSwitches.clear();
    m_customCategorySwitches.clear();
    m_customCategoryHeaderItems.clear();
    m_customCategoryOperationIds.clear();
    QStringList customCategoryOrder;
    for (const auto &operationId : std::as_const(m_customPermissionOperationIds)) {
        if (const auto *contract = AutomationWire::findPublicTool(operationId);
            contract && !contract->category.isEmpty()) {
            if (!m_customCategoryOperationIds.contains(contract->category))
                customCategoryOrder.append(contract->category);
            m_customCategoryOperationIds[contract->category].append(operationId);
        }
    }
    QList<OptionListCard *> customCategoryCards;
    if (m_customPermissionOperationIds.isEmpty()) {
        const auto customCard = new OptionListCard;
        customCard->setTitleVisible(false);
        customCard->addItem(tr("No public tools available"),
                            tr("Public tools appear here when the automation manifest is ready"));
        customCategoryCards.append(customCard);
    } else {
        for (const auto &category : std::as_const(customCategoryOrder)) {
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
            customCategoryCards.append(categoryCard);
        }
        refreshCategoryPermissionSwitches();
    }

    const auto customToolsSection = new QWidget;
    const auto customToolsLayout = new QVBoxLayout(customToolsSection);
    customToolsLayout->setContentsMargins({});
    customToolsLayout->setSpacing(6);
    const auto customToolsTitle = new QLabel(tr("Custom Tools"), customToolsSection);
    customToolsTitle->setObjectName(QStringLiteral("automationCustomToolsSectionTitle"));
    customToolsTitle->setProperty("optionsSectionTitle", true);
    customToolsTitle->setContentsMargins(10, 0, 0, 0);
    customToolsLayout->addWidget(customToolsTitle);
    for (auto *categoryCard : std::as_const(customCategoryCards))
        customToolsLayout->addWidget(categoryCard);

    m_readRoots = new PathEditor;
    m_readRoots->setPaths(option->readRoots);
    connect(m_readRoots, &PathEditor::pathsChanged, this, &AutomationPage::modifyOption);
    const auto readRootsCard = new OptionListCard;
    readRootsCard->setTitleVisible(false);
    m_readRootsItem = readRootsCard->addItem(
        tr("Allowed Read Folders"),
        tr("Limits MCP file reads, such as opening and importing, to the listed folders. "
           "Existing folders are saved as canonical paths."),
        m_readRoots);

    m_writeRoots = new PathEditor;
    m_writeRoots->setPaths(option->writeRoots);
    connect(m_writeRoots, &PathEditor::pathsChanged, this, &AutomationPage::modifyOption);
    const auto writeRootsCard = new OptionListCard;
    writeRootsCard->setTitleVisible(false);
    m_writeRootsItem = writeRootsCard->addItem(
        tr("Allowed Write Folders"),
        tr("Limits MCP file writes, such as saving and exporting, to the listed folders. "
           "Existing folders are saved as canonical paths."),
        m_writeRoots);

    const auto pathPermissionsSection = new QWidget;
    const auto pathPermissionsLayout = new QVBoxLayout(pathPermissionsSection);
    pathPermissionsLayout->setContentsMargins({});
    pathPermissionsLayout->setSpacing(6);
    const auto pathPermissionsTitle =
        new QLabel(tr("Read/Write Path Permissions"), pathPermissionsSection);
    pathPermissionsTitle->setObjectName(QStringLiteral("automationPathPermissionsSectionTitle"));
    pathPermissionsTitle->setProperty("optionsSectionTitle", true);
    pathPermissionsTitle->setContentsMargins(10, 0, 0, 0);
    pathPermissionsLayout->addWidget(pathPermissionsTitle);
    pathPermissionsLayout->addWidget(readRootsCard);
    pathPermissionsLayout->addWidget(writeRootsCard);

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(serverCard, 0, Qt::AlignTop);
    mainLayout->addWidget(connectionCard, 0, Qt::AlignTop);
    mainLayout->addWidget(accessCard, 0, Qt::AlignTop);
    mainLayout->addWidget(customToolsSection, 0, Qt::AlignTop);
    mainLayout->addWidget(pathPermissionsSection, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});
    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
