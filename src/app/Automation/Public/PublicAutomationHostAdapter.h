#ifndef PUBLICAUTOMATIONHOSTADAPTER_H
#define PUBLICAUTOMATIONHOSTADAPTER_H

#include "PublicAutomationRegistry.h"

class AppModel;
class SynthrtEngine;

namespace Automation {

    class CoreRuntime;

    PublicAutomationHostServices createPublicAutomationHostServices(CoreRuntime &runtime,
                                                                    AppModel *model,
                                                                    SynthrtEngine *synthrtEngine);

} // namespace Automation

#endif // PUBLICAUTOMATIONHOSTADAPTER_H
