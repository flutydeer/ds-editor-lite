#ifndef GPUCATALOG_H
#define GPUCATALOG_H

#include "Modules/Inference/Models/GpuInfo.h"

#include <QList>
#include <QString>

// 进程内缓存的 GPU 枚举结果，由自动化接口（settings.query 的 gpus、
// inference.get_capabilities 的 devices）共用。枚举逻辑与「按 provider 选择枚举器」的规则
// 仅在此处定义，以免各调用点分别探测，也避免在启动路径上执行探测（见
// docs/design/async-project-loading-design.md）。实际探测由 DmlGpuUtils（DXGI）和
// CudaGpuUtils（nvidia-smi）完成。
class GpuCatalog {
public:
    // providerId 采用 ExecutionProviderUtils::toString 的拼写（"CPU" / "DirectML" / "CUDA"）。
    // CPU 或未知 provider 返回空列表，与 GUI 下拉框及 InferEngine 中按 provider 区分设备的
    // 语义一致。每个 provider 在进程内只探测一次（空结果同样缓存），后续调用复用该结果。
    static QList<GpuInfo> forProvider(const QString &providerId);
};

#endif // GPUCATALOG_H
