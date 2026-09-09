#pragma once

#include <QDebug>
#include <QDir>

namespace TestSupport {
    inline bool useApplicationPluginRoot() {
#ifdef LITE_TEST_APPLICATION_PLUGIN_ROOT
        const auto root = QString::fromUtf8(LITE_TEST_APPLICATION_PLUGIN_ROOT);
        if (!QDir::isAbsolutePath(root) || !QDir(root).exists()) {
            qWarning() << "The application runtime plugin directory is unavailable:" << root;
            return false;
        }
        return qputenv("DSEL_TEST_PLUGIN_ROOT", root.toUtf8());
#else
        return true;
#endif
    }
}
