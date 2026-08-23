#include "AutomationPage.h"

#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/OptionsCard.h>
#include <lite/GUI/Controls/PathEditor.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>
#include <lite/GUI/Controls/SwitchButton.h>

#include <QHBoxLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#include <utility>

AutomationPage::AutomationPage(QWidget *parent) : IOptionPage(parent) {
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

    option->readRoots = m_readRoots->paths();
    option->writeRoots = m_writeRoots->paths();
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

    const auto accessCard = new OptionListCard(tr("Access Profile"));
    accessCard->addItem(
        tr("Profile"),
        sourceDescription(m_effectiveConfig.profileSource, QStringLiteral("--automation-profile")),
        m_profile);

    m_customPermissionSwitches.clear();
    const auto customCard = new OptionListCard(tr("Custom Permissions"));
    if (m_customPermissionOperationIds.isEmpty()) {
        customCard->addItem(tr("No public operations available"),
                            tr("Public operation permissions appear here when the automation manifest is ready"));
    } else {
        for (const auto &operationId : std::as_const(m_customPermissionOperationIds)) {
            auto *permissionSwitch =
                new SwitchButton(option->customPermissionEnabled(operationId));
            m_customPermissionSwitches.insert(operationId, permissionSwitch);
            connect(permissionSwitch, &SwitchButton::toggled, this, &AutomationPage::modifyOption);
            customCard->addItem(operationId, permissionSwitch);
        }
    }

    m_readRoots = new PathEditor;
    m_readRoots->setPaths(option->readRoots);
    connect(m_readRoots, &PathEditor::pathsChanged, this, &AutomationPage::modifyOption);
    const auto readRootsCard = new OptionsCard;
    const auto readRootsLayout = new QHBoxLayout;
    readRootsLayout->setContentsMargins(10, 10, 10, 10);
    readRootsLayout->addWidget(m_readRoots);
    readRootsCard->card()->setLayout(readRootsLayout);
    readRootsCard->setTitle(tr("Allowed Read Roots"));

    m_writeRoots = new PathEditor;
    m_writeRoots->setPaths(option->writeRoots);
    connect(m_writeRoots, &PathEditor::pathsChanged, this, &AutomationPage::modifyOption);
    const auto writeRootsCard = new OptionsCard;
    const auto writeRootsLayout = new QHBoxLayout;
    writeRootsLayout->setContentsMargins(10, 10, 10, 10);
    writeRootsLayout->addWidget(m_writeRoots);
    writeRootsCard->card()->setLayout(writeRootsLayout);
    writeRootsCard->setTitle(tr("Allowed Write Roots"));

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(serverCard, 0, Qt::AlignTop);
    mainLayout->addWidget(accessCard, 0, Qt::AlignTop);
    mainLayout->addWidget(customCard, 0, Qt::AlignTop);
    mainLayout->addWidget(readRootsCard, 0, Qt::AlignTop);
    mainLayout->addWidget(writeRootsCard, 0, Qt::AlignTop);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});
    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
