#include "ShellUtils.h"

#ifdef Q_OS_WIN
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QScopeGuard>

#  include <objbase.h>
#  include <shlobj_core.h>
#  include <vector>

#  pragma comment(lib, "ole32.lib")
#  pragma comment(lib, "shell32.lib")
#endif

#ifdef Q_OS_WIN
namespace {
    QHash<QString, QStringList> groupByDirectory(const QStringList &files) {
        QHash<QString, QStringList> result;
        for (const auto &file : files) {
            const QFileInfo fileInfo(file);
            result[fileInfo.absolutePath()].append(fileInfo.absoluteFilePath());
        }
        return result;
    }

    bool revealGroup(const QString &directory, const QStringList &files) {
        const auto comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
            return false;
        const auto comCleanup = qScopeGuard([comResult] {
            if (SUCCEEDED(comResult))
                CoUninitialize();
        });

        const auto nativeDirectory = QDir::toNativeSeparators(directory);
        const auto folderPidl =
            ILCreateFromPathW(reinterpret_cast<PCWSTR>(nativeDirectory.utf16()));
        if (!folderPidl)
            return false;
        const auto folderCleanup = qScopeGuard([folderPidl] { ILFree(folderPidl); });

        std::vector<PIDLIST_ABSOLUTE> itemPidls;
        itemPidls.reserve(files.size());
        const auto itemCleanup = qScopeGuard([&itemPidls] {
            for (const auto itemPidl : itemPidls)
                ILFree(itemPidl);
        });

        std::vector<PCUITEMID_CHILD> childPidls;
        childPidls.reserve(files.size());
        for (const auto &file : files) {
            const auto nativeFile = QDir::toNativeSeparators(file);
            const auto itemPidl =
                ILCreateFromPathW(reinterpret_cast<PCWSTR>(nativeFile.utf16()));
            if (!itemPidl)
                return false;
            itemPidls.push_back(itemPidl);
            childPidls.push_back(ILFindLastID(itemPidl));
        }

        const auto result = childPidls.empty()
                                ? E_INVALIDARG
                                : SHOpenFolderAndSelectItems(folderPidl,
                                                             static_cast<UINT>(childPidls.size()),
                                                             childPidls.data(), 0);
        return SUCCEEDED(result);
    }

    void revealSingleFile(const QString &file) {
        QProcess::startDetached(
            QStringLiteral("explorer.exe"),
            {QStringLiteral("/select,%1").arg(QDir::toNativeSeparators(file))});
    }
}
#endif

namespace ShellUtils {
    void revealFiles(const QStringList &files) {
#ifdef Q_OS_WIN
        const auto groups = groupByDirectory(files);
        for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
            if (!revealGroup(it.key(), it.value()))
                revealSingleFile(it.value().constFirst());
        }
#else
        Q_UNUSED(files);
#endif
    }
}
