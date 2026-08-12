#include "StartupArguments.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>

namespace StartupArguments {

    QStringList projectFilePaths() {
        QStringList paths;
        const auto args = QApplication::arguments().mid(1);
        paths.reserve(args.size());
        for (const auto &argument : args) {
            if (!argument.isEmpty())
                paths.append(QDir::cleanPath(QFileInfo(argument).absoluteFilePath()));
        }
        return paths;
    }

} // namespace StartupArguments
