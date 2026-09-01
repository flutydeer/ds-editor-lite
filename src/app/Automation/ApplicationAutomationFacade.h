#ifndef APPLICATIONAUTOMATIONFACADE_H
#define APPLICATIONAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <functional>

namespace Automation {

    enum class ApplicationTerminationMode {
        Exit,
        Restart,
    };

    enum class ApplicationTerminationSavePolicy {
        Prompt,
        RejectUnsaved,
        Discard,
    };

    enum class ApplicationTerminationRequestResult {
        Accepted,
        Busy,
        UnsavedChanges,
        Unavailable,
    };

    struct ApplicationInfoDto {
        QString name;
        QString version;
        QString platform;
        QString buildId;

        friend bool operator==(const ApplicationInfoDto &, const ApplicationInfoDto &) = default;
    };

    struct ApplicationRuntimeServices {
        std::function<ApplicationInfoDto()> info;
        std::function<ApplicationTerminationRequestResult(ApplicationTerminationMode,
                                                          ApplicationTerminationSavePolicy)>
            requestTermination;
    };

    class ApplicationAutomationFacade final {
    public:
        ApplicationAutomationFacade(AutomationDispatcher &dispatcher,
                                    ApplicationRuntimeServices services = {});

        AutomationResult<ApplicationInfoDto> getInfo();
        AutomationResult<GuiMutationResult> requestTermination(const GuiCommandContext &context,
                                                               ApplicationTerminationMode mode,
                                                               bool discardChanges = false);

    private:
        AutomationDispatcher &m_dispatcher;
        ApplicationRuntimeServices m_services;
    };

} // namespace Automation

#endif // APPLICATIONAUTOMATIONFACADE_H
