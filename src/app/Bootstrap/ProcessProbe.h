#ifndef PROCESSPROBE_H
#define PROCESSPROBE_H

#include <QtGlobal>

#if defined(Q_OS_WIN)
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#endif

/// Check process liveness. Unreaped Unix processes still count as alive.
namespace ProcessProbe {

    inline bool isAlive(const qint64 pid) {
        if (pid <= 0)
            return false;
#if defined(Q_OS_WIN)
        HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
        if (handle == nullptr)
            return GetLastError() == ERROR_ACCESS_DENIED;
        const bool alive = WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
        CloseHandle(handle);
        return alive;
#else
        return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
    }

}

#endif // PROCESSPROBE_H
