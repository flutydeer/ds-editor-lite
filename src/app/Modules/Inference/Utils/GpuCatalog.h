#ifndef GPUCATALOG_H
#define GPUCATALOG_H

#include "Modules/Inference/Models/GpuInfo.h"

#include <QList>
#include <QString>

// The in-process cache of GPU enumeration, shared by the automation interfaces (the gpus of
// settings.query and the devices of inference.get_capabilities). The enumeration logic and the rule
// "which enumerator to use per provider" are defined only here, so that call sites do not probe
// separately and the startup path does not probe at all (see
// docs/design/async-project-loading-design.md). The actual probing is done by DmlGpuUtils (DXGI)
// and CudaGpuUtils (nvidia-smi).
class GpuCatalog {
public:
    // providerId uses the spelling of ExecutionProviderUtils::toString ("CPU" / "DirectML" /
    // "CUDA"). CPU or an unknown provider returns an empty list, consistent with the GUI combo box
    // and with the per-provider device distinction in InferEngine. Each provider is probed only
    // once per process (an empty result is cached as well), and later calls reuse that result.
    static QList<GpuInfo> forProvider(const QString &providerId);
};

#endif // GPUCATALOG_H
