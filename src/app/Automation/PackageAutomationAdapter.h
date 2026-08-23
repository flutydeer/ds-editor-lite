#ifndef PACKAGEAUTOMATIONADAPTER_H
#define PACKAGEAUTOMATIONADAPTER_H

#include "PackageAutomationFacade.h"

class PackageManager;

namespace Automation {

    PackageRuntimeServices createPackageAutomationServices(PackageManager *manager);

} // namespace Automation

#endif // PACKAGEAUTOMATIONADAPTER_H
