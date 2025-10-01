#ifndef CUDAGPUUTILS_H
#define CUDAGPUUTILS_H

#include "Modules/Inference/Models/GpuInfo.h"

#include <QList>

class CudaGpuUtils {
public:
    static GpuInfo getGpuByIndex(int index);
    static QList<GpuInfo> getGpuList();
    static GpuInfo getGpuByPciDeviceVendorId(unsigned int pciDeviceId, unsigned int pciVendorId,
                                             int indexHint = 0);
    static GpuInfo getGpuByPciDeviceVendorIdString(const QString &idString, int indexHint = 0);
    static GpuInfo getGpuByUuid(const QString &uuid);
    static GpuInfo getRecommendedGpu();
    static void setNvidiaSmiPath(const QString &path);
};

#ifndef ONNXRUNTIME_ENABLE_CUDA
inline GpuInfo CudaGpuUtils::getGpuByIndex(int index) {
    Q_UNUSED(index)
    return {};
}

inline QList<GpuInfo> CudaGpuUtils::getGpuList() {
    return {};
}

inline GpuInfo CudaGpuUtils::getGpuByPciDeviceVendorId(unsigned int pciDeviceId,
                                                       unsigned int pciVendorId, int indexHint) {
    Q_UNUSED(pciDeviceId)
    Q_UNUSED(pciVendorId)
    Q_UNUSED(indexHint)
    return {};
}

inline GpuInfo CudaGpuUtils::getGpuByPciDeviceVendorIdString(const QString &idString,
                                                             int indexHint) {
    Q_UNUSED(idString)
    Q_UNUSED(indexHint)
    return {};
}

inline GpuInfo CudaGpuUtils::getGpuByUuid(const QString &uuid) {
    Q_UNUSED(uuid)
    return {};
}

inline GpuInfo CudaGpuUtils::getRecommendedGpu() {
    return {};
}

inline void CudaGpuUtils::setNvidiaSmiPath(const QString &path) {
}

#endif

#endif // CUDAGPUUTILS_H
