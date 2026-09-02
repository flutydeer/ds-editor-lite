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

        ApplicationTerminationSavePolicy terminationSavePolicy(const InvocationSource source,
                                                               const bool discardChanges) {
            switch (source) {
                case InvocationSource::TrustedGui:
                    return ApplicationTerminationSavePolicy::Prompt;
                case InvocationSource::InternalAutomation:
                case InvocationSource::PublicMcp:
                case InvocationSource::PublicJsonRpc:
                case InvocationSource::PublicTaskContinuation:
                case InvocationSource::Test:
                    return discardChanges ? ApplicationTerminationSavePolicy::Discard
                                          : ApplicationTerminationSavePolicy::RejectUnsaved;
            }
            Q_UNREACHABLE_RETURN(ApplicationTerminationSavePolicy::RejectUnsaved);
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

    AutomationResult<ApplicationMutationResult>
        ApplicationAutomationFacade::requestTermination(const ApplicationCommandContext &context,
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
        return m_dispatcher.dispatchApplicationCommand<ApplicationMutationResult>(
            operationId, context, [this, context, mode, discardChanges](const bool validateOnly) {
                if (!m_services.requestTermination)
                    return AutomationResult<ApplicationMutationResult>(unavailable());
                if (!validateOnly) {
                    const auto savePolicy = terminationSavePolicy(context.source, discardChanges);
                    switch (m_services.requestTermination(mode, savePolicy)) {
                        case ApplicationTerminationRequestResult::Accepted:
                            break;
                        case ApplicationTerminationRequestResult::Busy:
                            return AutomationResult<ApplicationMutationResult>(busy(
                                QStringLiteral("The editor is busy and cannot terminate now")));
                        case ApplicationTerminationRequestResult::UnsavedChanges:
                            return AutomationResult<ApplicationMutationResult>(
                                busy(QStringLiteral(
                                         "The current document has unsaved changes; set "
                                         "discard_changes to true to terminate without saving"),
                                     QStringLiteral("discard_changes")));
                        case ApplicationTerminationRequestResult::Unavailable:
                            return AutomationResult<ApplicationMutationResult>(unavailable());
                    }
                }
                return AutomationResult<ApplicationMutationResult>({
                    .changed = true,
                    .validatedOnly = validateOnly,
                });
            });
    }

} // namespace Automation
