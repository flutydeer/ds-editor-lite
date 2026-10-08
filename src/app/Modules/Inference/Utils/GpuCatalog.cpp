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

    // The probe runs outside the lock: DXGI has to walk the adapters and nvidia-smi has to start a
    // process, which may take hundreds of milliseconds, and that must not block other threads. With
    // concurrent calls the same provider may be probed more than once, and every result is the
    // same.
    QList<GpuInfo> enumerated;
    if (providerId == ExecutionProviderUtils::toString(ExecutionProvider::Cuda))
        enumerated = CudaGpuUtils::getGpuList();
    else if (providerId == ExecutionProviderUtils::toString(ExecutionProvider::DirectML))
        enumerated = DmlGpuUtils::getGpuList();

    QMutexLocker locker(&g_cacheMutex);
    g_cache.insert(providerId, enumerated);
    return enumerated;
}
