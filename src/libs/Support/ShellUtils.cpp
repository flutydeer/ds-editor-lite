#include "ShellUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QProcess>

#ifdef Q_OS_WIN
#  include <shlobj_core.h>
#  include <vector>

#  pragma comment(lib, "shell32.lib")
#endif

namespace {
    QHash<QString, QStringList> groupByDirectory(const QStringList &files) {
        QHash<QString, QStringList> result;
        for (const auto &file : files) {
            const QFileInfo fileInfo(file);
            result[fileInfo.absolutePath()].append(fileInfo.absoluteFilePath());
        }
        return result;
    }

#ifdef Q_OS_WIN
    bool revealGroup(const QString &directory, const QStringList &files) {
        const auto folderPidl = ILCreateFromPathW(reinterpret_cast<PCWSTR>(directory.utf16()));
        if (!folderPidl)
            return false;

        std::vector<PIDLIST_ABSOLUTE> itemPidls;
        std::vector<PCUITEMID_CHILD> childPidls;
        for (const auto &file : files) {
            const auto itemPidl = ILCreateFromPathW(reinterpret_cast<PCWSTR>(file.utf16()));
            if (!itemPidl)
                continue;
            itemPidls.push_back(itemPidl);
            // Child PIDLs point into itemPidls and must not be freed separately.
            childPidls.push_back(ILFindLastID(itemPidl));
        }

        const auto result =
            childPidls.empty()
                ? E_INVALIDARG
                : SHOpenFolderAndSelectItems(folderPidl, static_cast<UINT>(childPidls.size()),
                                             childPidls.data(), 0);

        for (const auto itemPidl : itemPidls)
            ILFree(itemPidl);
        ILFree(folderPidl);
        return SUCCEEDED(result);
    }

    void revealSingleFile(const QString &file) {
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                {QStringLiteral("/select,"), QDir::toNativeSeparators(file)});
    }
#endif
}

namespace ShellUtils {
    void revealFiles(const QStringList &files) {
        const auto groups = groupByDirectory(files);
        for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
#ifdef Q_OS_WIN
            if (!revealGroup(it.key(), it.value()))
                revealSingleFile(it.value().constFirst());
#elif defined(Q_OS_MACOS)
            QProcess::startDetached(QStringLiteral("open"), {it.key()});
#else
            QProcess::startDetached(QStringLiteral("xdg-open"), {it.key()});
#endif
        }
    }
}
