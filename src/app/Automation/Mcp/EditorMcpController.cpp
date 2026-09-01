#include "EditorMcpController.h"

#include "Automation/EditorAutomationRuntimeStatus.h"
#include "Automation/Native/NativeJsonRpcDispatcher.h"
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
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <cstdlib>
#include <utility>

namespace Automation {
    namespace {
        AutomationWire::Mcp::ImplementationInfo editorServerInfo() {
            return {
                .name = QString::fromLatin1(LiteProductMetadata::ProductName),
                .version = QString::fromLatin1(LiteProductMetadata::Version),
                .description = QStringLiteral("%1 embedded MCP server")
                                   .arg(QString::fromLatin1(LiteProductMetadata::ProductName)),
                .websiteUrl = QString::fromLatin1(LiteProductMetadata::ProductUrl),
            };
        }

        AutomationHttpRoutes desiredRoutes(const AppHostMode hostMode, const bool mcpEnabled) {
            return {
                .mcp = mcpEnabled,
                .native = hostMode == AppHostMode::Headless,
            };
        }
    }

    EditorMcpController::EditorMcpController(CoreRuntime &runtime, AppOptions &options,
                                             SingleInstanceCoordinator &coordinator,
                                             const AppHostMode hostMode,
                                             StartupArguments::AutomationOverrides overrides,
                                             PublicAutomationHostServices hostServices,
                                             QObject *parent)
        : QObject(parent), m_options(options), m_coordinator(coordinator), m_hostMode(hostMode),
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
        hostServices.hostMode = appHostModeName(m_hostMode);
        if (!hostServices.windowStatus && m_hostMode == AppHostMode::Headless) {
            hostServices.windowStatus = [] { return QJsonArray{}; };
        } else if (!hostServices.windowStatus) {
            hostServices.windowStatus = [&runtime] {
                const auto version = runtime.documentVersion();
                if (!runtime.windowId())
                    return QJsonArray{};
                return QJsonArray{
                    QJsonObject{
                                {QStringLiteral("window_id"), runtime.windowId()->toString()},
                                {QStringLiteral("document_id"), version.documentId.toString()},
                                {QStringLiteral("active"), true},
                                }
                };
            };
        }

        m_registry = std::make_unique<PublicAutomationRegistry>(
            runtime, m_accessPolicy, m_fileGuard, m_admissionController, std::move(hostServices));
        m_dispatcher = std::make_unique<McpRequestDispatcher>(*m_registry, editorServerInfo());
        m_nativeDispatcher = std::make_unique<NativeJsonRpcDispatcher>(*m_registry);
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
            },
            [this](const QJsonValue &message, const QString &clientId) {
                return m_nativeDispatcher->dispatch(message, clientId);
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
            if (contract.minimumControlLevel != AutomationWire::ControlLevel::L0)
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

    QString EditorMcpController::nativeEndpoint() const {
        return m_server ? m_server->nativeEndpoint() : QString{};
    }

    QString EditorMcpController::errorString() const {
        return m_lastError;
    }

    void EditorMcpController::applyConfiguration() {
        if (m_shuttingDown || m_fatalHeadlessFailure)
            return;
        m_effectiveConfig =
            StartupArguments::effectiveAutomationConfig(*m_options.automation(), m_overrides);
        m_accessPolicy.update(controlLevelFor(m_effectiveConfig.controlLevel),
                              enabledCustomOperations(*m_options.automation()));

        const auto roots = m_fileGuard.setConfiguredRoots(m_options.automation()->accessRoots);
        const auto routes = desiredRoutes(m_hostMode, m_effectiveConfig.mcpEnabled);
        if (!routes.mcp && !routes.native) {
            transitionAfterStop(PendingTransition::Disabled);
            return;
        }
        if (!m_registry->isComplete()) {
            handleConfigurationError(QStringLiteral("Public automation registry is incomplete"));
            return;
        }
        if (!roots) {
            handleConfigurationError(roots.getError().message);
            return;
        }
        if (m_effectiveConfig.controlPort == 0) {
            handleConfigurationError(
                QStringLiteral("Automation control port must be between 1 and 65535"));
            return;
        }

        if (m_server->isListening() && m_boundRequestedPort == m_effectiveConfig.controlPort) {
            if (m_server->routes() != routes) {
                QString error;
                if (!m_server->setRoutes(routes, error)) {
                    handleConfigurationError(error);
                    return;
                }
            }
            m_admissionController.setAccepting(true);
            m_lastError.clear();
            if (routes.mcp) {
                publishStatus(SingleInstanceAutomationState::ServerReady, true,
                              m_server->endpoint());
            } else {
                publishStatus(SingleInstanceAutomationState::ServerDisabled, false);
            }
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

    AutomationWire::ControlLevel
        EditorMcpController::controlLevelFor(const AutomationOption::ControlLevel level) {
        switch (level) {
            case AutomationOption::ControlLevel::L1:
                return AutomationWire::ControlLevel::L1;
            case AutomationOption::ControlLevel::L2:
                return AutomationWire::ControlLevel::L2;
            case AutomationOption::ControlLevel::L3:
                return AutomationWire::ControlLevel::L3;
            case AutomationOption::ControlLevel::Custom:
                return AutomationWire::ControlLevel::Custom;
        }
        return AutomationWire::ControlLevel::L1;
    }

    QSet<QString> EditorMcpController::enabledCustomOperations(const AutomationOption &option) {
        QSet<QString> result;
        for (auto it = option.customPermissions.constBegin();
             it != option.customPermissions.constEnd(); ++it) {
            const auto *contract = AutomationWire::findPublicTool(it.key());
            if (it.value() && contract &&
                contract->minimumControlLevel != AutomationWire::ControlLevel::L0) {
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
            publishStatus(SingleInstanceAutomationState::ServerStopping,
                          m_effectiveConfig.mcpEnabled);
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
                m_lastError.clear();
                publishStatus(SingleInstanceAutomationState::ServerDisabled, false);
                return;
            case PendingTransition::Error: {
                m_boundRequestedPort = 0;
                m_lastError = std::exchange(m_pendingError, QString{});
                publishStatus(SingleInstanceAutomationState::Error, m_effectiveConfig.mcpEnabled,
                              {}, m_lastError);
                return;
            }
            case PendingTransition::Start:
                startServer();
                return;
        }
    }

    void EditorMcpController::startServer() {
        const auto routes = desiredRoutes(m_hostMode, m_effectiveConfig.mcpEnabled);
        if (routes.mcp)
            publishStatus(SingleInstanceAutomationState::ServerStarting, true);
        QString error;
        if (!m_server->start(m_effectiveConfig.controlPort, routes, error)) {
            m_admissionController.setAccepting(false);
            if (m_hostMode == AppHostMode::Headless && m_everStarted) {
                failHeadlessRuntime(std::move(error));
            } else {
                m_lastError = error;
                publishStatus(SingleInstanceAutomationState::Error, routes.mcp, {}, error);
            }
            return;
        }
        m_boundRequestedPort = m_effectiveConfig.controlPort;
        m_everStarted = true;
        m_admissionController.setAccepting(true);
        m_lastError.clear();
        if (routes.mcp) {
            publishStatus(SingleInstanceAutomationState::ServerReady, true, m_server->endpoint());
        } else {
            publishStatus(SingleInstanceAutomationState::ServerDisabled, false);
        }
    }

    void EditorMcpController::handleConfigurationError(QString error) {
        if (m_hostMode == AppHostMode::Headless && m_everStarted) {
            failHeadlessRuntime(std::move(error));
            return;
        }
        transitionAfterStop(PendingTransition::Error, std::move(error));
    }

    void EditorMcpController::failHeadlessRuntime(QString error) {
        if (m_fatalHeadlessFailure)
            return;
        m_fatalHeadlessFailure = true;
        m_pendingTransition = PendingTransition::None;
        m_pendingError.clear();
        m_admissionController.setAccepting(false);
        m_lastError = std::move(error);
        publishStatus(SingleInstanceAutomationState::Error, m_effectiveConfig.mcpEnabled, {},
                      m_lastError);
        QTextStream(stderr) << "Fatal headless automation error: " << m_lastError << Qt::endl;
        if (auto *application = QCoreApplication::instance()) {
            QTimer::singleShot(0, application, [] { QCoreApplication::exit(EXIT_FAILURE); });
        }
    }

    void EditorMcpController::publishStatus(const SingleInstanceAutomationState state,
                                            const bool enabled, QString endpoint, QString error) {
        if (auto *application = QCoreApplication::instance()) {
            application->setProperty(AutomationRuntimeStatus::StateProperty,
                                     SingleInstanceProtocol::automationStateName(state));
            application->setProperty(AutomationRuntimeStatus::EndpointProperty, endpoint);
            application->setProperty(AutomationRuntimeStatus::ErrorProperty, error);
        }
        auto status = m_coordinator.automationState();
        status.state = state;
        status.applicationVersion = QString::fromLatin1(LiteProductMetadata::Version);
        status.buildId = QString::fromLatin1(LITE_GIT_LAST_COMMIT_HASH);
        status.hostMode = appHostModeName(m_hostMode);
        status.serverEnabled = enabled;
        status.serverEndpoint = std::move(endpoint);
        status.error = std::move(error);
        m_coordinator.updateAutomationState(status);
    }

} // namespace Automation
