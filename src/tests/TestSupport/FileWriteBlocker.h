#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtTest/QTest>

namespace TestSupport {
    class FileWriteBlocker final {
    public:
        explicit FileWriteBlocker(const QString &path)
            : m_path(path), m_backup(path + QStringLiteral(".test-write-backup")) {
        }

        ~FileWriteBlocker() {
            if (!restore())
                QTest::qFail(qPrintable(QStringLiteral("Failed to restore %1").arg(m_path)),
                             __FILE__, __LINE__);
        }

        FileWriteBlocker(const FileWriteBlocker &) = delete;
        FileWriteBlocker &operator=(const FileWriteBlocker &) = delete;

        bool block() {
            if (!QFile::rename(m_path, m_backup))
                return false;
            m_ownsBackup = true;
            // A directory at the file path makes writes fail consistently on every platform.
            return QDir().mkdir(m_path);
        }

        bool restore() {
            if (!m_ownsBackup)
                return true;
            if (QFileInfo(m_path).isDir() && !QDir().rmdir(m_path))
                return false;
            if (!QFile::rename(m_backup, m_path))
                return false;
            m_ownsBackup = false;
            return true;
        }

    private:
        QString m_path;
        QString m_backup;
        bool m_ownsBackup = false;
    };
}
