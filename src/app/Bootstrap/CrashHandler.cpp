#include "CrashHandler.h"

#include "Utils/AppDumpDirectory.h"

#include <QApplication>

#ifdef LITE_ENABLE_BREAKPAD
#  include <QBreakpadHandler.h>
#endif

#ifdef _WIN32
#  include <Windows.h>
#endif

CrashHandler::CrashHandler() {
#ifdef LITE_ENABLE_BREAKPAD
    m_handler = std::make_unique<QBreakpadHandler>();
    m_handler->setDumpPath(AppDumpDirectory::resolveDumpDirectory());

    QBreakpadHandler::UniqueExtraHandler = []() {
        ::MessageBoxW(nullptr, L"Crash detected", L"Error", MB_OK | MB_ICONERROR);
    };
#endif
}

CrashHandler::~CrashHandler() = default;
