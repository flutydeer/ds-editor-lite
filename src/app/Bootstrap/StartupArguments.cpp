#include "StartupArguments.h"

#include <QApplication>

namespace StartupArguments {

    QString projectFilePath() {
        const auto args = QApplication::arguments();
        if (args.count() == 2 && !args.at(1).isEmpty())
            return args.at(1);
        return {};
    }

} // namespace StartupArguments
