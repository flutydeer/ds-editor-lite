//
// Created by KCKT0112 on 2025/12/13.
//

#ifndef RECENTFILESMANAGER_H
#define RECENTFILESMANAGER_H

#define recentFilesManager RecentFilesManager::instance()

#include "Utils/Singleton.h"

#include <QStringList>
#include <QUrl>

class RecentFilesManager final {
private:
    explicit RecentFilesManager();
    ~RecentFilesManager();

public:
    LITE_SINGLETON_DECLARE_INSTANCE(RecentFilesManager)
    Q_DISABLE_COPY_MOVE(RecentFilesManager)

public:
    void addFile(const QString &filePath);
    [[nodiscard]] QStringList files() const;
    void load();
    void save();
    void syncToSystemRecentDocuments();

private:
    static void addToSystemRecent(const QString &filePath);

private:
    QStringList m_recentFiles;
    QString m_configPath;
    static constexpr int MAX_RECENT_FILES = 30;
};

#endif // RECENTFILESMANAGER_H
