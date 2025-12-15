//
// Created by fluty on 2024/12/XX.
//

#include "RecentFilesManager.h"

#include "Utils/JsonUtils.h"
#include "Utils/Log.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>
#include <utility>
#include <QtCore/qsystemdetection.h>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <shlobj.h>
#elif defined(Q_OS_MAC)
// Implemented in Objective-C++ file
extern void addToMacRecentDocument(const QString &filePath);
#endif

#define CLASS_NAME "RecentFilesManager"

RecentFilesManager::RecentFilesManager() {
    const QDir configDir(
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first());
    if (!configDir.exists()) {
        if (configDir.mkpath("."))
            Log::d(CLASS_NAME, "Successfully created config directory");
        else
            Log::e(CLASS_NAME, "Failed to create config directory");
    }
    m_configPath = configDir.absoluteFilePath("recentFiles.json");
    load();
    // Sync application recent files to system recent documents on startup
    syncToSystemRecentDocuments();
}

RecentFilesManager::~RecentFilesManager() {
    save();
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(RecentFilesManager)

void RecentFilesManager::addFile(const QString &filePath) {
    // Remove existing identical files
    m_recentFiles.removeAll(filePath);
    // Add to beginning
    m_recentFiles.prepend(filePath);
    // Limit up to 30 items
    if (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles = m_recentFiles.mid(0, MAX_RECENT_FILES);
    }
    save();

    // Add to system recent documents (macOS Dock / Windows Jump List)
    addToSystemRecent(filePath);
}

QStringList RecentFilesManager::files() const {
    return m_recentFiles;
}

void RecentFilesManager::load() {
    if (!QFile::exists(m_configPath)) {
        m_recentFiles = {};
        return;
    }

    QJsonObject obj;
    if (!JsonUtils::load(m_configPath, obj)) {
        Log::e(CLASS_NAME, "Failed to load recent files");
        m_recentFiles = {};
        return;
    }

    if (obj.contains("files") && obj["files"].isArray()) {
        QStringList files;
        const auto arr = obj["files"].toArray();
        for (auto item : std::as_const(arr)) {
            if (item.isString()) {
                const QString filePath = item.toString();
                // Keep only existing files
                if (QFile::exists(filePath)) {
                    files.append(filePath);
                }
            }
        }
        m_recentFiles = std::move(files);
    } else {
        m_recentFiles = {};
    }
}

void RecentFilesManager::save() {
    QJsonObject obj;
    obj["files"] = QJsonArray::fromStringList(m_recentFiles);

    if (!JsonUtils::save(m_configPath, obj)) {
        Log::e(CLASS_NAME, "Failed to save recent files");
    }
}

void RecentFilesManager::syncToSystemRecentDocuments() {
    // Sync application recent files to system recent documents
    // This ensures system-level integration (macOS Dock / Windows Jump List)
    // shows the same files as the application menu
    for (const auto &filePath : m_recentFiles) {
        if (QFile::exists(filePath)) {
            addToSystemRecent(filePath);
        }
    }
}

void RecentFilesManager::addToSystemRecent(const QString &filePath) {
#if defined(Q_OS_WIN)
    // Windows Jump List integration via shell API
    SHAddToRecentDocs(SHARD_PATHW, filePath.toStdWString().c_str());
#elif defined(Q_OS_MAC)
    addToMacRecentDocument(filePath);
#else
    Q_UNUSED(filePath);
#endif
}
