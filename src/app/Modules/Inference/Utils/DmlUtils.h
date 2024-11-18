//
// Created by fluty on 24-9-28.
//

#ifndef DMLUTILS_H
#define DMLUTILS_H

#include "Modules/Inference/Models/GpuInfo.h"

#include <QList>

class DmlUtils {
public:
    static GpuInfo getGpuByIndex(int index);
    static QList<GpuInfo> getDirectXGPUs();
    static GpuInfo getGpuById(unsigned int deviceId, unsigned int vendorId, int indexHint = 0);
    static GpuInfo getGpuByIdString(const QString &idString, int indexHint = 0);
    static GpuInfo getRecommendedGpu();
};

#ifndef _WIN32
    inline GpuInfo DmlUtils::getGpuByIndex(int index) {
        Q_UNUSED(index)
        return {};
    }

    inline QList<GpuInfo> DmlUtils::getDirectXGPUs() {
        return {};
    }

    inline GpuInfo DmlUtils::getGpuById(unsigned int deviceId, unsigned int vendorId, int indexHint) {
        Q_UNUSED(deviceId)
        Q_UNUSED(vendorId)
        Q_UNUSED(indexHint)
        return {};
    }

    inline GpuInfo DmlUtils::getGpuByIdString(const QString &idString, int indexHint) {
        Q_UNUSED(idString)
        Q_UNUSED(indexHint)
        return {};
    }

    inline GpuInfo DmlUtils::getRecommendedGpu() {
        return {};
    }
#endif

#endif // DMLUTILS_H
