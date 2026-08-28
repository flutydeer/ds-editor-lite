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

        AutomationError busy(const QString &message, QString fieldPath = {}) {
            AutomationError error;
            error.code = AutomationErrorCode::Busy;
            error.message = message;
            error.fieldPath = std::move(fieldPath);
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
                                                        const ApplicationTerminationMode mode,
                                                        const bool discardChanges) {
        if (mode != ApplicationTerminationMode::Exit &&
            mode != ApplicationTerminationMode::Restart) {
            return AutomationError::invalidArgument(
                QStringLiteral("mode"), QStringLiteral("Application termination mode is invalid"));
        }
        const auto operationId = mode == ApplicationTerminationMode::Exit
                                     ? OperationIds::application::request_exit
                                     : OperationIds::application::request_restart;
        return m_dispatcher.dispatchGuiCommand<GuiMutationResult>(
            operationId, context, [this, context, mode, discardChanges](const bool validateOnly) {
                if (!m_services.requestTermination)
                    return AutomationResult<GuiMutationResult>(unavailable());
                if (!validateOnly) {
                    const auto savePolicy =
                        context.source == InvocationSource::TrustedGui
                            ? ApplicationTerminationSavePolicy::Prompt
                            : discardChanges ? ApplicationTerminationSavePolicy::Discard
                                             : ApplicationTerminationSavePolicy::RejectUnsaved;
                    switch (m_services.requestTermination(mode, savePolicy)) {
                        case ApplicationTerminationRequestResult::Accepted:
                            break;
                        case ApplicationTerminationRequestResult::Busy:
                            return AutomationResult<GuiMutationResult>(busy(
                                QStringLiteral("The editor is busy and cannot terminate now")));
                        case ApplicationTerminationRequestResult::UnsavedChanges:
                            return AutomationResult<GuiMutationResult>(busy(
                                QStringLiteral("The current document has unsaved changes; set "
                                               "discard_changes to true to terminate without saving"),
                                QStringLiteral("discard_changes")));
                        case ApplicationTerminationRequestResult::Unavailable:
                            return AutomationResult<GuiMutationResult>(unavailable());
                    }
                }
                return AutomationResult<GuiMutationResult>({
                    .windowId = context.windowId,
                    .changed = true,
                    .validatedOnly = validateOnly,
                });
            });
    }

} // namespace Automation
