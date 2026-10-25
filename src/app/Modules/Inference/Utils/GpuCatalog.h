#ifndef GPUCATALOG_H
#define GPUCATALOG_H

#include "Modules/Inference/Models/GpuInfo.h"

#include <QList>
#include <QString>

// Shared GPU enumeration cache using DXGI for DirectML and nvidia-smi for CUDA.
class GpuCatalog {
public:
    // CPU and unknown providers return no devices. Cached results expire after five seconds.
    static QList<GpuInfo> forProvider(const QString &providerId);
};

#endif // GPUCATALOG_H
