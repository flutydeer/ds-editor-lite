#include "GpuCatalog.h"

#include "Modules/Inference/ExecutionProvider.h"
#include "Modules/Inference/Utils/CudaGpuUtils.h"
#include "Modules/Inference/Utils/DmlGpuUtils.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <chrono>

namespace {
    QMutex g_cacheMutex;
    struct CachedGpus {
        QList<GpuInfo> devices;
        std::chrono::steady_clock::time_point updated;
    };
    QHash<QString, CachedGpus> g_cache;
    constexpr auto cacheLifetime = std::chrono::seconds(5);
} // namespace

QList<GpuInfo> GpuCatalog::forProvider(const QString &providerId) {
    if (providerId.isEmpty() ||
        providerId == ExecutionProviderUtils::toString(ExecutionProvider::Cpu))
        return {};

    {
        QMutexLocker locker(&g_cacheMutex);
        const auto cached = g_cache.constFind(providerId);
        if (cached != g_cache.constEnd() &&
            std::chrono::steady_clock::now() - cached->updated < cacheLifetime)
            return cached->devices;
    }

    // Probe outside the mutex because device enumeration can block.
    QList<GpuInfo> enumerated;
    if (providerId == ExecutionProviderUtils::toString(ExecutionProvider::Cuda))
        enumerated = CudaGpuUtils::getGpuList();
    else if (providerId == ExecutionProviderUtils::toString(ExecutionProvider::DirectML))
        enumerated = DmlGpuUtils::getGpuList();

    QMutexLocker locker(&g_cacheMutex);
    g_cache.insert(providerId, {enumerated, std::chrono::steady_clock::now()});
    return enumerated;
}
