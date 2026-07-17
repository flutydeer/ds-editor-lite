#ifndef STARTUPARGUMENTS_H
#define STARTUPARGUMENTS_H

#include <QString>

namespace StartupArguments {

    // Returns the project file path passed on the command line,
    // or an empty string if none was given.
    QString projectFilePath();

} // namespace StartupArguments

#endif // STARTUPARGUMENTS_H
