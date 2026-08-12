#ifndef STARTUPARGUMENTS_H
#define STARTUPARGUMENTS_H

#include <QStringList>

namespace StartupArguments {

    // Returns project paths passed on the command line as cleaned absolute paths.
    QStringList projectFilePaths();

} // namespace StartupArguments

#endif // STARTUPARGUMENTS_H
