#ifndef PUBLICAUTOMATIONHOSTADAPTER_H
#define PUBLICAUTOMATIONHOSTADAPTER_H

#include "PublicAutomationRegistry.h"

class AppModel;

namespace Automation {

    class CoreRuntime;

    PublicAutomationHostServices createPublicAutomationHostServices(CoreRuntime &runtime,
                                                                    AppModel *model);

} // namespace Automation

#endif // PUBLICAUTOMATIONHOSTADAPTER_H
