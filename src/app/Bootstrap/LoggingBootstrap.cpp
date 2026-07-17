#include "LoggingBootstrap.h"

#include "Modules/Inference/Utils/DmlGpuUtils.h"
#include "Utils/Log.h"

#include <QDir>
#include <QStandardPaths>

namespace LoggingBootstrap {

    // Log GPU info at the application layer to keep Log (Utils) free of module dependencies
    static void logGpuInfo() {
        qInfo() << "-------- GPU Info Begin --------";
        for (const auto &gpu : DmlGpuUtils::getGpuList())
            qInfo() << gpu.index << gpu.description;
        qInfo() << "--------- GPU Info End ---------";
    }

    void init() {
        // 设置日志等级和过滤器
        QDir appDataDir(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first());
        if (!appDataDir.exists()) {
            if (!appDataDir.mkpath("."))
                qFatal() << "Failed to create app data directory";
        }
        // Log::setLogDirectory(
        //     QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first() +
        //     "/Logs");
        Log::setConsoleLogLevel(Log::Debug);
        // Log::setConsoleTagFilter({"InferPipeline"});
        Log::logSystemInfo();
        logGpuInfo();
    }

} // namespace LoggingBootstrap
