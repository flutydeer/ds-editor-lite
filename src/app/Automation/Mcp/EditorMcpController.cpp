#include "EditorMcpController.h"

#include "EditorMcpRuntimeStatus.h"
#include "McpHttpServer.h"
#include "McpRequestDispatcher.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/SingleInstanceCoordinator.h"
#include "Model/AppOptions/AppOptions.h"

#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/BuildInfo.h>
#include <lite/ProductMetadata.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QThread>
#include <QUuid>

#include <utility>

namespace Automation {
    namespace {
        AutomationWire::Mcp::ImplementationInfo editorServerInfo() {
            return {
                .name = QString::fromLatin1(LiteProductMetadata::ProductName),
                .version = QString::fromLatin1(LiteProductMetadata::Version),
                .description = QStringLiteral("DS Editor Lite embedded MCP server"),
                .websiteUrl = QString::fromLatin1(LiteProductMetadata::ProductUrl),
            };
        }
    }

    EditorMcpController::EditorMcpController(CoreRuntime &runtime, AppOptions &options,
                                             SingleInstanceCoordinator &coordinator,
                                             StartupArguments::AutomationOverrides overrides,
                                             PublicAutomationHostServices hostServices,
                                             QObject *parent)
        : QObject(parent), m_options(options), m_coordinator(coordinator),
          m_overrides(std::move(overrides)) {
        const auto instanceId = QUuid(m_coordinator.automationState().editorInstanceId);
        if (hostServices.editorInstanceId.isNull())
            hostServices.editorInstanceId = instanceId;
        if (!hostServices.documentStatus) {
            hostServices.documentStatus = [&runtime] {
                const auto version = runtime.documentVersion();
                return QJsonArray{
                    QJsonObject{
                                {QStringLiteral("document_id"), version.documentId.toString()},
                                {QStringLiteral("revision"), static_cast<qint64>(version.revision)},
                                {QStringLiteral("active"), true},
                                }
                };
            };
        }
        if (!hostServices.windowStatus) {
            hostServices.windowStatus = [&runtime] {
                const auto version = runtime.documentVersion();
                return QJsonArray{
                    QJsonObject{
                                {QStringLiteral("window_id"), runtime.windowId().toString()},
                                {QStringLiteral("document_id"), version.documentId.toString()},
                                {QStringLiteral("active"), true},
                                }
                };
            };
        }

        m_registry = std::make_unique<PublicAutomationRegistry>(
            runtime, m_accessPolicy, m_fileGuard, m_admissionController, std::move(hostServices));
        m_dispatcher = std::make_unique<McpRequestDispatcher>(*m_registry, editorServerInfo());
        m_server = std::make_unique<McpHttpServer>(
            this,
            [this](const AutomationWire::Mcp::RequestEnvelope &request, const QString &clientId) {
                if (QThread::currentThread() == thread())
                    return m_dispatcher->dispatch(request, clientId);

                QJsonObject response;
                const auto invoked = QMetaObject::invokeMethod(
                    this,
                    [this, &request, &clientId, &response] {
                        response = m_dispatcher->dispatch(request, clientId);
                    },
                    Qt::BlockingQueuedConnection);
                if (!invoked) {
                    return AutomationWire::Mcp::makeErrorResponse(
                        request.id, {AutomationWire::Mcp::InternalError,
                                     QStringLiteral("Editor automation thread is unavailable")});
                }
                return response;
            });

        connect(m_server.get(), &McpHttpServer::stopped, this,
                &EditorMcpController::completePendingTransition);
        connect(&m_options, &AppOptions::optionsChanged, this,
                [this](const AppOptionsGlobal::Option option) {
                    if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::Automation)
                        applyConfiguration();
                });
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
                &EditorMcpController::shutdown);
        applyConfiguration();
    }

    EditorMcpController::~EditorMcpController() {
        shutdown();
    }

    QStringList EditorMcpController::customPermissionOperationIds() const {
        QStringList result;
        for (const auto &contract : AutomationWire::publicToolContracts()) {
            if (contract.minimumProfile != AutomationWire::AutomationProfile::L0)
                result.append(contract.operationId);
        }
        result.sort();
        return result;
    }

    PublicAutomationRegistry &EditorMcpController::registry() {
        return *m_registry;
    }

    const PublicAutomationRegistry &EditorMcpController::registry() const {
        return *m_registry;
    }

    void EditorMcpController::applyConfiguration() {
        if (m_shuttingDown)
            return;
        m_effectiveConfig =
            StartupArguments::effectiveAutomationConfig(*m_options.automation(), m_overrides);
        m_accessPolicy.update(profileFor(m_effectiveConfig.profile),
                              enabledCustomOperations(*m_options.automation()));

        const auto roots = m_fileGuard.setConfiguredRoots(m_options.automation()->readRoots,
                                                          m_options.automation()->writeRoots);
        if (!m_effectiveConfig.mcpEnabled) {
            transitionAfterStop(PendingTransition::Disabled);
            return;
        }
        if (!m_registry->isComplete()) {
            transitionAfterStop(PendingTransition::Error,
                                QStringLiteral("Public automation registry is incomplete"));
            return;
        }
        if (!roots) {
            transitionAfterStop(PendingTransition::Error, roots.getError().message);
            return;
        }

        if (m_server->isListening() && m_boundRequestedPort == m_effectiveConfig.controlPort) {
            m_admissionController.setAccepting(true);
            publishStatus(SingleInstanceAutomationState::McpReady, true, m_server->endpoint());
            return;
        }
        transitionAfterStop(PendingTransition::Start);
    }

    void EditorMcpController::shutdown() {
        if (m_shuttingDown)
            return;
        m_shuttingDown = true;
        m_pendingTransition = PendingTransition::None;
        m_admissionController.setAccepting(false);
        publishStatus(SingleInstanceAutomationState::EditorStopping, false);
        if (m_server->isListening() || m_server->isStopping()) {
            m_server->requestStop();
            m_server->stop();
        }
    }

    AutomationWire::AutomationProfile
        EditorMcpController::profileFor(const AutomationOption::Profile profile) {
        switch (profile) {
            case AutomationOption::Profile::L1:
                return AutomationWire::AutomationProfile::L1;
            case AutomationOption::Profile::L2:
                return AutomationWire::AutomationProfile::L2;
            case AutomationOption::Profile::L3:
                return AutomationWire::AutomationProfile::L3;
            case AutomationOption::Profile::Custom:
                return AutomationWire::AutomationProfile::Custom;
        }
        return AutomationWire::AutomationProfile::L1;
    }

    QSet<QString> EditorMcpController::enabledCustomOperations(const AutomationOption &option) {
        QSet<QString> result;
        for (auto it = option.customPermissions.constBegin();
             it != option.customPermissions.constEnd(); ++it) {
            const auto *contract = AutomationWire::findPublicTool(it.key());
            if (it.value() && contract &&
                contract->minimumProfile != AutomationWire::AutomationProfile::L0) {
                result.insert(it.key());
            }
        }
        return result;
    }

    void EditorMcpController::transitionAfterStop(const PendingTransition transition,
                                                  QString error) {
        m_pendingTransition = transition;
        m_pendingError = std::move(error);
        m_admissionController.setAccepting(false);
        if (m_server->isListening() || m_server->isStopping()) {
            publishStatus(SingleInstanceAutomationState::McpStopping, m_effectiveConfig.mcpEnabled);
            m_server->requestStop();
            return;
        }
        completePendingTransition();
    }

    void EditorMcpController::completePendingTransition() {
        if (m_shuttingDown)
            return;
        const auto transition = std::exchange(m_pendingTransition, PendingTransition::None);
        switch (transition) {
            case PendingTransition::None:
                return;
            case PendingTransition::Disabled:
                m_boundRequestedPort = 0;
                publishStatus(SingleInstanceAutomationState::McpDisabled, false);
                return;
            case PendingTransition::Error:
                m_boundRequestedPort = 0;
                publishStatus(SingleInstanceAutomationState::Error, true, {},
                              std::exchange(m_pendingError, QString{}));
                return;
            case PendingTransition::Start:
                startServer();
                return;
        }
    }

    void EditorMcpController::startServer() {
        publishStatus(SingleInstanceAutomationState::McpStarting, true);
        QString error;
        if (!m_server->start(m_effectiveConfig.controlPort, error)) {
            m_admissionController.setAccepting(false);
            publishStatus(SingleInstanceAutomationState::Error, true, {}, error);
            return;
        }
        m_boundRequestedPort = m_effectiveConfig.controlPort;
        m_admissionController.setAccepting(true);
        publishStatus(SingleInstanceAutomationState::McpReady, true, m_server->endpoint());
    }

    void EditorMcpController::publishStatus(const SingleInstanceAutomationState state,
                                            const bool enabled, QString endpoint, QString error) {
        if (auto *application = QCoreApplication::instance()) {
            application->setProperty(McpRuntimeStatus::StateProperty,
                                     SingleInstanceProtocol::automationStateName(state));
            application->setProperty(McpRuntimeStatus::EndpointProperty, endpoint);
            application->setProperty(McpRuntimeStatus::ErrorProperty, error);
        }
        auto status = m_coordinator.automationState();
        status.state = state;
        status.applicationVersion = QString::fromLatin1(LiteProductMetadata::Version);
        status.buildId = QString::fromLatin1(LITE_GIT_LAST_COMMIT_HASH);
        status.hostMode = QStringLiteral("gui");
        status.mcpEnabled = enabled;
        status.mcpEndpoint = std::move(endpoint);
        status.error = std::move(error);
        m_coordinator.updateAutomationState(status);
    }

} // namespace Automation
