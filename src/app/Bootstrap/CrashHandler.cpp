#include "CrashHandler.h"

#ifdef APPLICATION_ENABLE_BREAKPAD
#  include <QApplication>
#  include <QBreakpadHandler.h>
#endif

CrashHandler::CrashHandler() {
#ifdef APPLICATION_ENABLE_BREAKPAD
    m_handler = std::make_unique<QBreakpadHandler>();
    m_handler->setDumpPath(QApplication::applicationDirPath() + "/dumps");

    QBreakpadHandler::UniqueExtraHandler = []() {
        // Do something when crash occurs.
    };
#endif
}

CrashHandler::~CrashHandler() = default;
