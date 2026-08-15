#include "CrashHandler.h"

#include <QApplication>
#include <QStandardPaths>

#ifdef LITE_ENABLE_BREAKPAD
#  include <QBreakpadHandler.h>
#endif

CrashHandler::CrashHandler() {
#ifdef LITE_ENABLE_BREAKPAD
    m_handler = std::make_unique<QBreakpadHandler>();
    m_handler->setDumpPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                           QStringLiteral("/Dumps"));

    QBreakpadHandler::UniqueExtraHandler = []() {
        // Do something when crash occurs.
    };
#endif
}

CrashHandler::~CrashHandler() = default;
