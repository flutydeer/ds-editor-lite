//
// Created by fluty on 24-9-28.
//

#ifndef DMLGPUUTILS_H
#define DMLGPUUTILS_H

#include "Modules/Inference/Models/GpuInfo.h"

#include <QList>

class DmlGpuUtils {
public:
    static GpuInfo getGpuByIndex(int index);
    static QList<GpuInfo> getGpuList();
    static GpuInfo getGpuById(unsigned int deviceId, unsigned int vendorId, int indexHint = 0);
    static GpuInfo getGpuByIdString(const QString &idString, int indexHint = 0);
    static GpuInfo getRecommendedGpu();
};

#ifndef _WIN32
    inline GpuInfo DmlGpuUtils::getGpuByIndex(int index) {
        Q_UNUSED(index)
        return {};
    }

    inline QList<GpuInfo> DmlGpuUtils::getGpuList() {
        return {};
    }

    inline GpuInfo DmlGpuUtils::getGpuById(unsigned int deviceId, unsigned int vendorId, int indexHint) {
        Q_UNUSED(deviceId)
        Q_UNUSED(vendorId)
        Q_UNUSED(indexHint)
        return {};
    }

    inline GpuInfo DmlGpuUtils::getGpuByIdString(const QString &idString, int indexHint) {
        Q_UNUSED(idString)
        Q_UNUSED(indexHint)
        return {};
    }

    inline GpuInfo DmlGpuUtils::getRecommendedGpu() {
        return {};
    }
#endif

#endif // DMLGPUUTILS_H
