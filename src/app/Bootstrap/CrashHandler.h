#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include <memory>

class QBreakpadHandler;

// RAII wrapper around the Breakpad crash handler. When the application is
// built without LITE_ENABLE_BREAKPAD, construction is a no-op.
class CrashHandler {
public:
    CrashHandler();
    ~CrashHandler();

private:
#ifdef LITE_ENABLE_BREAKPAD
    std::unique_ptr<QBreakpadHandler> m_handler;
#endif
};

#endif // CRASHHANDLER_H
