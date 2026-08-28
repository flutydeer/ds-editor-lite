#include "ApplicationAutomationFacade.h"
#include "OperationIds.h"

#include <utility>

namespace Automation {
    namespace {
        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.message = QStringLiteral("Application lifecycle host is unavailable");
            return error;
        }
    }

    ApplicationAutomationFacade::ApplicationAutomationFacade(AutomationDispatcher &dispatcher,
                                                             ApplicationRuntimeServices services)
        : m_dispatcher(dispatcher), m_services(std::move(services)) {
    }

    AutomationResult<ApplicationInfoDto> ApplicationAutomationFacade::getInfo() {
        return m_dispatcher.dispatchApplicationQuery<ApplicationInfoDto>(
            OperationIds::application::get_info, [this] {
                if (!m_services.info)
                    return AutomationResult<ApplicationInfoDto>(unavailable());
                return AutomationResult<ApplicationInfoDto>(m_services.info());
            });
    }

    AutomationResult<GuiMutationResult>
        ApplicationAutomationFacade::requestTermination(const GuiCommandContext &context,
                                                        const ApplicationTerminationMode mode) {
        if (mode != ApplicationTerminationMode::Exit &&
            mode != ApplicationTerminationMode::Restart) {
            return AutomationError::invalidArgument(
                QStringLiteral("mode"), QStringLiteral("Application termination mode is invalid"));
        }
        const auto operationId = mode == ApplicationTerminationMode::Exit
                                     ? OperationIds::application::request_exit
                                     : OperationIds::application::request_restart;
        return m_dispatcher.dispatchGuiCommand<GuiMutationResult>(
            operationId, context, [this, context, mode](const bool validateOnly) {
                if (!m_services.requestTermination)
                    return AutomationResult<GuiMutationResult>(unavailable());
                if (!validateOnly && !m_services.requestTermination(mode))
                    return AutomationResult<GuiMutationResult>(unavailable());
                return AutomationResult<GuiMutationResult>({
                    .windowId = context.windowId,
                    .changed = true,
                    .validatedOnly = validateOnly,
                });
            });
    }

} // namespace Automation
