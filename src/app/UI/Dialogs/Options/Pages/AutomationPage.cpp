#include "AutomationPage.h"

#include "Automation/Mcp/EditorMcpRuntimeStatus.h"
#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"

#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/OptionsCardItem.h>
#include <lite/GUI/Controls/PathEditor.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>
#include <lite/GUI/Controls/SwitchButton.h>

#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
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
    operationIds.sort();
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
    for (auto it = m_customCategorySwitches.constBegin();
         it != m_customCategorySwitches.constEnd(); ++it) {
        const auto operationIds = m_customCategoryOperationIds.value(it.key());
        const auto allEnabled = !operationIds.isEmpty() &&
                                std::all_of(operationIds.cbegin(), operationIds.cend(),
                                            [this](const QString &operationId) {
                                                const auto operation =
                                                    m_customPermissionSwitches.value(operationId);
                                                return operation && operation->value();
                                            });
        const QSignalBlocker blocker(it.value());
        it.value()->setValue(allEnabled);
    }
}

void AutomationPage::refreshRuntimeStatus() {
    const auto *application = QCoreApplication::instance();
    if (!application)
        return;
    const auto state = application->property(Automation::McpRuntimeStatus::StateProperty).toString();
    const auto endpoint =
        application->property(Automation::McpRuntimeStatus::EndpointProperty).toString();
    const auto error = application->property(Automation::McpRuntimeStatus::ErrorProperty).toString();
    if (m_runtimeStateItem)
        m_runtimeStateItem->setDescription(state.isEmpty() ? tr("Not initialized")
                                                           : runtimeStateDescription(state));
    if (m_runtimeEndpointItem) {
        m_runtimeEndpointItem->setDescription(endpoint.isEmpty() ? tr("Not listening") : endpoint);
    }
    if (m_runtimeErrorItem)
        m_runtimeErrorItem->setDescription(error.isEmpty() ? tr("No error") : error);
}

void AutomationPage::modifyOption() {
    auto *option = appOptions->automation();
    if (m_effectiveConfig.mcpEnabledSource == StartupArguments::ConfigSource::Persisted)
        option->mcpEnabled = m_mcpEnabled->value();
    if (m_effectiveConfig.controlPortSource == StartupArguments::ConfigSource::Persisted)
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
}

QWidget *AutomationPage::createContentWidget() {
    const auto widget = new QWidget;
    auto *option = appOptions->automation();
    const auto parsedArguments = StartupArguments::parseApplicationArguments();
    m_effectiveConfig = StartupArguments::effectiveAutomationConfig(
        *option, parsedArguments.isValid() ? parsedArguments.automation
                                           : StartupArguments::AutomationOverrides{});

    m_mcpEnabled = new SwitchButton(m_effectiveConfig.mcpEnabled);
    m_mcpEnabled->setEnabled(m_effectiveConfig.mcpEnabledSource ==
                             StartupArguments::ConfigSource::Persisted);
    connect(m_mcpEnabled, &SwitchButton::toggled, this, &AutomationPage::modifyOption);

    m_controlPort = new SVS::ExpressionSpinBox;
    m_controlPort->setRange(0, 65535);
    m_controlPort->setValue(m_effectiveConfig.controlPort);
    m_controlPort->setEnabled(m_effectiveConfig.controlPortSource ==
                              StartupArguments::ConfigSource::Persisted);
    connect(m_controlPort, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &AutomationPage::modifyOption);

    m_profile = new ComboBox;
    m_profile->addItem(tr("L1 - Basic Editing"), static_cast<int>(AutomationOption::Profile::L1));
    m_profile->addItem(tr("L2 - Complete Creation"), static_cast<int>(AutomationOption::Profile::L2));
    m_profile->addItem(tr("L3 - Full Control"), static_cast<int>(AutomationOption::Profile::L3));
    m_profile->addItem(tr("Custom"), static_cast<int>(AutomationOption::Profile::Custom));
    m_profile->setCurrentIndex(m_profile->findData(static_cast<int>(m_effectiveConfig.profile)));
    m_profile->setEnabled(m_effectiveConfig.profileSource ==
                          StartupArguments::ConfigSource::Persisted);
    connect(m_profile, &ComboBox::currentIndexChanged, this, &AutomationPage::modifyOption);

    const auto serverCard = new OptionListCard(tr("MCP Server"));
    serverCard->addItem(tr("Enable MCP Server"),
                        sourceDescription(m_effectiveConfig.mcpEnabledSource,
                                          m_effectiveConfig.mcpEnabled
                                              ? QStringLiteral("--mcp")
                                              : QStringLiteral("--no-mcp")),
                        m_mcpEnabled);
    serverCard->addItem(
        tr("Control Port"),
        sourceDescription(m_effectiveConfig.controlPortSource, QStringLiteral("--control-port")),
        m_controlPort);
    m_runtimeStateItem = serverCard->addItem(tr("Runtime Status"), QString{});
    m_runtimeEndpointItem = serverCard->addItem(tr("Current Endpoint"), QString{});
    m_runtimeErrorItem = serverCard->addItem(tr("Last Error"), QString{});
    refreshRuntimeStatus();

    const auto accessCard = new OptionListCard(tr("Access Profile"));
    accessCard->addItem(
        tr("Profile"),
        sourceDescription(m_effectiveConfig.profileSource, QStringLiteral("--automation-profile")),
        m_profile);

    m_customPermissionSwitches.clear();
    m_customCategorySwitches.clear();
    m_customCategoryOperationIds.clear();
    for (const auto &operationId : std::as_const(m_customPermissionOperationIds)) {
        if (const auto *contract = AutomationWire::findPublicTool(operationId);
            contract && !contract->category.isEmpty()) {
            m_customCategoryOperationIds[contract->category].append(operationId);
        }
    }
    const auto categoryCard = new OptionListCard(tr("Custom Permission Categories"));
    for (auto it = m_customCategoryOperationIds.constBegin();
         it != m_customCategoryOperationIds.constEnd(); ++it) {
        const auto operationIds = it.value();
        const auto allEnabled = std::all_of(
            operationIds.cbegin(), operationIds.cend(),
            [option](const QString &operationId) {
                return option->customPermissionEnabled(operationId);
            });
        auto *categorySwitch = new SwitchButton(allEnabled);
        m_customCategorySwitches.insert(it.key(), categorySwitch);
        connect(categorySwitch, &SwitchButton::toggled, this,
                [this, operationIds](const bool enabled) {
                    for (const auto &operationId : operationIds) {
                        if (auto *operation = m_customPermissionSwitches.value(operationId)) {
                            const QSignalBlocker blocker(operation);
                            operation->setValue(enabled);
                        }
                    }
                    modifyOption();
                });
        categoryCard->addItem(categoryDisplayName(it.key()), categorySwitch);
    }
    const auto customCard = new OptionListCard(tr("Custom Permissions"));
    if (m_customPermissionOperationIds.isEmpty()) {
        customCard->addItem(tr("No public operations available"),
                            tr("Public operation permissions appear here when the automation manifest is ready"));
    } else {
        for (const auto &operationId : std::as_const(m_customPermissionOperationIds)) {
            auto *permissionSwitch =
                new SwitchButton(option->customPermissionEnabled(operationId));
            m_customPermissionSwitches.insert(operationId, permissionSwitch);
            connect(permissionSwitch, &SwitchButton::toggled, this, [this] {
                refreshCategoryPermissionSwitches();
                modifyOption();
            });
            customCard->addItem(operationId, permissionSwitch);
        }
    }

    m_readRoots = new PathEditor;
    m_readRoots->setPaths(option->readRoots);
    connect(m_readRoots, &PathEditor::pathsChanged, this, &AutomationPage::modifyOption);
    const auto readRootsCard = new OptionListCard(tr("Allowed Read Folders"));
    m_readRootsItem = readRootsCard->addItem(
        tr("Folders"),
        tr("Limits MCP file reads, such as opening and importing, to the listed folders. "
           "Existing folders are saved as canonical paths."),
        m_readRoots);

    m_writeRoots = new PathEditor;
    m_writeRoots->setPaths(option->writeRoots);
    connect(m_writeRoots, &PathEditor::pathsChanged, this, &AutomationPage::modifyOption);
    const auto writeRootsCard = new OptionListCard(tr("Allowed Write Folders"));
    m_writeRootsItem = writeRootsCard->addItem(
        tr("Folders"),
        tr("Limits MCP file writes, such as saving and exporting, to the listed folders. "
           "Existing folders are saved as canonical paths."),
        m_writeRoots);

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(serverCard, 0, Qt::AlignTop);
    mainLayout->addWidget(accessCard, 0, Qt::AlignTop);
    mainLayout->addWidget(categoryCard, 0, Qt::AlignTop);
    mainLayout->addWidget(customCard, 0, Qt::AlignTop);
    mainLayout->addWidget(readRootsCard, 0, Qt::AlignTop);
    mainLayout->addWidget(writeRootsCard, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});
    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
