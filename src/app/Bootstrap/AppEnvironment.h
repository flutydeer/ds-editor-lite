#ifndef APPENVIRONMENT_H
#define APPENVIRONMENT_H

#include "AppHostMode.h"

// Application-wide Qt environment setup split around QCoreApplication construction.
namespace AppEnvironment {

    // Must be called before the selected Qt application object is constructed. GUI-only platform
    // attributes are skipped for the QCore host.
    void preInit(AppHostMode hostMode);

    // Must be called after the selected Qt application object is constructed. Common metadata is
    // installed for both hosts; styles, fonts, and widget defaults are GUI-only.
    void postInit(AppHostMode hostMode);

} // namespace AppEnvironment

#endif // APPENVIRONMENT_H
