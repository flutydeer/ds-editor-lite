#ifndef EXTRACTIONAUTOMATIONADAPTER_H
#define EXTRACTIONAUTOMATIONADAPTER_H

#include "ExtractionAutomationFacade.h"

class AppOptions;

namespace Automation {

    ExtractionRuntimeServices createExtractionAutomationServices(AppOptions *options);

} // namespace Automation

#endif // EXTRACTIONAUTOMATIONADAPTER_H
