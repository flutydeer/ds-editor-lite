#ifndef INFERCACHEUTILS_H
#define INFERCACHEUTILS_H

#include <QSet>
#include <QSharedPointer>
#include <QString>

#include <QList>

class QMutex;

namespace InferCacheUtils {
    class CacheWriteGuard final {
    public:
        explicit CacheWriteGuard(const QString &key);
        ~CacheWriteGuard();

        Q_DISABLE_COPY_MOVE(CacheWriteGuard)

    private:
        QSharedPointer<QMutex> m_mutex;
    };

    struct CacheFileInfo {
        QString fileName;
        QString category; // "acoustic" | "duration" | "pitch" | "variance"
        bool isInput = false;
        qint64 size = 0;
    };

    struct CacheStats {
        QList<CacheFileInfo> files;
        qint64 totalBytes = 0;
    };

    struct CleanResult {
        int deletedCount = 0;
        qint64 deletedBytes = 0;
        int retainedActiveCount = 0; // 当前工程使用中（登记/状态引用），跳过
        int retainedLockedCount = 0; // 删除失败（被占用/只读）
    };

    // 扫描缓存目录（纯文件操作，可在工作线程调用；目录不存在返回空 stats）
    CacheStats scanCache(const QString &cacheDir);

    // 登记本会话产生的缓存文件绝对路径（必须主线程调用；任务完成回调）
    void registerCacheFile(const QString &absolutePath);

    // 已登记的缓存文件绝对路径集合（进程级，主线程调用）
    QSet<QString> registeredCacheFiles();

    // 收集当前工程活跃缓存文件绝对路径（必须主线程调用，访问 appModel）：
    // = 登记集合 ∪ 当前工程 Success 片段引用集合
    QSet<QString> collectActiveCacheFiles();

    // 清理：删除非活跃缓存文件（纯文件操作，可在工作线程调用）
    CleanResult cleanCache(const QString &cacheDir, const QSet<QString> &activeFiles);

} // namespace InferCacheUtils

#endif // INFERCACHEUTILS_H
