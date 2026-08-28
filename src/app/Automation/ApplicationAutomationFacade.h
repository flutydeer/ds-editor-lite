#ifndef APPLICATIONAUTOMATIONFACADE_H
#define APPLICATIONAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <functional>

namespace Automation {

    enum class ApplicationTerminationMode {
        Exit,
        Restart,
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
        std::function<bool(ApplicationTerminationMode)> requestTermination;
    };

    class ApplicationAutomationFacade final {
    public:
        ApplicationAutomationFacade(AutomationDispatcher &dispatcher,
                                    ApplicationRuntimeServices services = {});

        AutomationResult<ApplicationInfoDto> getInfo();
        AutomationResult<GuiMutationResult> requestTermination(const GuiCommandContext &context,
                                                               ApplicationTerminationMode mode);

    private:
        AutomationDispatcher &m_dispatcher;
        ApplicationRuntimeServices m_services;
    };

} // namespace Automation

#endif // APPLICATIONAUTOMATIONFACADE_H
