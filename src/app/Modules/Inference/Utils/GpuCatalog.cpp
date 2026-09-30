#include "GpuCatalog.h"

#include "Modules/Inference/ExecutionProvider.h"
#include "Modules/Inference/Utils/CudaGpuUtils.h"
#include "Modules/Inference/Utils/DmlGpuUtils.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

namespace {
    QMutex g_cacheMutex;
    QHash<QString, QList<GpuInfo>> g_cache;
} // namespace

QList<GpuInfo> GpuCatalog::forProvider(const QString &providerId) {
    if (providerId.isEmpty() ||
        providerId == ExecutionProviderUtils::toString(ExecutionProvider::Cpu))
        return {};

    {
        QMutexLocker locker(&g_cacheMutex);
        const auto cached = g_cache.constFind(providerId);
        if (cached != g_cache.constEnd())
            return *cached;
    }

    // 探测在锁外执行：DXGI 需要遍历 adapter，nvidia-smi 需要启动进程，可能耗时数百毫秒，
    // 不应阻塞其他线程。并发调用时同一 provider 最多可能被探测多次，各次结果相同。
    QList<GpuInfo> enumerated;
    if (providerId == ExecutionProviderUtils::toString(ExecutionProvider::Cuda))
        enumerated = CudaGpuUtils::getGpuList();
    else if (providerId == ExecutionProviderUtils::toString(ExecutionProvider::DirectML))
        enumerated = DmlGpuUtils::getGpuList();

    QMutexLocker locker(&g_cacheMutex);
    g_cache.insert(providerId, enumerated);
    return enumerated;
}
