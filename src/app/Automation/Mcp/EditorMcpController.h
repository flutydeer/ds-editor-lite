#ifndef EDITORMCPCONTROLLER_H
#define EDITORMCPCONTROLLER_H

#include "../Public/AdmissionController.h"
#include "../Public/AutomationAccessPolicy.h"
#include "../Public/AutomationFileGuard.h"
#include "../Public/PublicAutomationRegistry.h"
#include "Bootstrap/SingleInstanceProtocol.h"
#include "Bootstrap/StartupArguments.h"

#include <QObject>

#include <memory>

class AppOptions;
class SingleInstanceCoordinator;

namespace Automation {

    class CoreRuntime;
    class McpHttpServer;
    class McpRequestDispatcher;

    class EditorMcpController final : public QObject {
        Q_OBJECT

    public:
        EditorMcpController(CoreRuntime &runtime, AppOptions &options,
                            SingleInstanceCoordinator &coordinator,
                            StartupArguments::AutomationOverrides overrides,
                            PublicAutomationHostServices hostServices = {},
                            QObject *parent = nullptr);
        ~EditorMcpController() override;

        Q_DISABLE_COPY_MOVE(EditorMcpController)

        [[nodiscard]] QStringList customPermissionOperationIds() const;
        [[nodiscard]] PublicAutomationRegistry &registry();
        [[nodiscard]] const PublicAutomationRegistry &registry() const;

        void applyConfiguration();
        void shutdown();

    private:
        enum class PendingTransition {
            None,
            Disabled,
            Start,
            Error,
        };

        static AutomationWire::ControlLevel controlLevelFor(AutomationOption::ControlLevel level);
        static QSet<QString> enabledCustomOperations(const AutomationOption &option);

        void transitionAfterStop(PendingTransition transition, QString error = {});
        void completePendingTransition();
        void startServer();
        void publishStatus(SingleInstanceAutomationState state, bool enabled, QString endpoint = {},
                           QString error = {});

        AppOptions &m_options;
        SingleInstanceCoordinator &m_coordinator;
        StartupArguments::AutomationOverrides m_overrides;
        AutomationAccessPolicy m_accessPolicy;
        AutomationFileGuard m_fileGuard;
        AdmissionController m_admissionController;
        std::unique_ptr<PublicAutomationRegistry> m_registry;
        std::unique_ptr<McpRequestDispatcher> m_dispatcher;
        std::unique_ptr<McpHttpServer> m_server;
        StartupArguments::EffectiveAutomationConfig m_effectiveConfig;
        PendingTransition m_pendingTransition = PendingTransition::None;
        QString m_pendingError;
        quint16 m_boundRequestedPort = 0;
        bool m_shuttingDown = false;
    };

} // namespace Automation

#endif // EDITORMCPCONTROLLER_H
