#ifndef EXTRACTIONAUTOMATIONADAPTER_H
#define EXTRACTIONAUTOMATIONADAPTER_H

#include "ExtractionAutomationFacade.h"

class AppOptions;
class TaskManager;

namespace Automation {

    ExtractionRuntimeServices createExtractionAutomationServices(AppOptions *options,
                                                                 TaskManager *taskRuntime);

} // namespace Automation

#endif // EXTRACTIONAUTOMATIONADAPTER_H
