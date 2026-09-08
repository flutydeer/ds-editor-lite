#pragma once

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <lite/ProductMetadata.h>

namespace AppDataPaths {
    inline QString testRoot() {
#ifdef LITE_TEST_DATA_ROOT
        const auto root = qEnvironmentVariable("DSEL_TEST_DATA_ROOT");
        if (!root.isEmpty() && QFileInfo(root).isAbsolute())
            return QDir::cleanPath(root);
#endif
        return {};
    }

    inline QString applicationData() {
        const auto root = testRoot();
        if (!root.isEmpty()) {
            return QDir(root).filePath(QStringLiteral("%1/%2").arg(
                QString::fromLatin1(LiteProductMetadata::Publisher),
                QString::fromLatin1(LiteProductMetadata::ProductName)));
        }
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
}
