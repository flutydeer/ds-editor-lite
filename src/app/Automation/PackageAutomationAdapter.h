#ifndef PACKAGEAUTOMATIONADAPTER_H
#define PACKAGEAUTOMATIONADAPTER_H

#include "PackageAutomationFacade.h"

class PackageManager;
class AppOptions;

namespace Automation {

    PackageRuntimeServices createPackageAutomationServices(PackageManager *manager,
                                                           AppOptions *options = nullptr);

} // namespace Automation

#endif // PACKAGEAUTOMATIONADAPTER_H
