#include "LoggingBootstrap.h"
#include "AppDataPaths.h"

#include "Modules/Inference/Utils/DmlGpuUtils.h"
#include <lite/Support/Log.h>

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
        QDir appDataDir(AppDataPaths::applicationData());
        if (!appDataDir.exists()) {
            if (!appDataDir.mkpath("."))
                qFatal() << "Failed to create app data directory";
        }
#ifdef LITE_ENABLE_FILE_LOG
        Log::setLogDirectory(AppDataPaths::applicationData() + "/Logs");
#endif
        Log::setConsoleLogLevel(Log::Debug);
        // Log::setConsoleTagFilter({"InferPipeline"});
        Log::logSystemInfo();
        logGpuInfo();
    }

} // namespace LoggingBootstrap
