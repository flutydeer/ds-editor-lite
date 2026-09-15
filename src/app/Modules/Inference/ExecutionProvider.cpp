#include "Modules/Inference/ExecutionProvider.h"

#include <atomic>

namespace {
    std::atomic<ExecutionProvider> g_effectiveProvider{ExecutionProvider::Cpu};
}

QString ExecutionProviderUtils::toString(const ExecutionProvider provider) noexcept {
    switch (provider) {
        case ExecutionProvider::Cpu:
            return QStringLiteral("CPU");
        case ExecutionProvider::DirectML:
            return QStringLiteral("DirectML");
        case ExecutionProvider::Cuda:
            return QStringLiteral("CUDA");
    }
    return QStringLiteral("CPU");
}

std::optional<ExecutionProvider> ExecutionProviderUtils::fromString(const QString &value) noexcept {
    if (value == QStringLiteral("CPU"))
        return ExecutionProvider::Cpu;
    if (value == QStringLiteral("DirectML"))
        return ExecutionProvider::DirectML;
    if (value == QStringLiteral("CUDA"))
        return ExecutionProvider::Cuda;
    return std::nullopt;
}

bool ExecutionProviderUtils::availableInBuild(const ExecutionProvider provider) noexcept {
    switch (provider) {
        case ExecutionProvider::Cpu:
            return true;
        case ExecutionProvider::DirectML:
#if defined(Q_OS_WIN)
            return true;
#else
            return false;
#endif
        case ExecutionProvider::Cuda:
            // Mirrors InferenceOption::cudaExecutionProviderAvailable(); keep both
            // in sync when another provider joins this policy.
#if defined(ONNXRUNTIME_ENABLE_CUDA)
            return true;
#else
            return false;
#endif
    }
    return false;
}

bool ExecutionProviderUtils::requiresGpu(const ExecutionProvider provider) noexcept {
    return provider != ExecutionProvider::Cpu;
}

ExecutionProviderUtils::Resolution
    ExecutionProviderUtils::resolve(const QString &persisted, const DeviceAvailability &devices) {
    const auto requested = fromString(persisted);
    if (!requested) {
        return {ExecutionProvider::Cpu, true,
                persisted.isEmpty()
                    ? QStringLiteral("no execution provider is configured")
                    : QStringLiteral("unknown execution provider '%1'").arg(persisted)};
    }
    if (*requested == ExecutionProvider::Cpu)
        return {ExecutionProvider::Cpu, false, {}};
    if (!availableInBuild(*requested)) {
        return {ExecutionProvider::Cpu, true,
                QStringLiteral("%1 is not available in this build").arg(toString(*requested))};
    }
    if (!devices.gpuFound) {
        return {
            ExecutionProvider::Cpu, true,
            QStringLiteral("%1 has no usable device on this machine").arg(toString(*requested))};
    }
    return {*requested, false, {}};
}

ExecutionProvider ExecutionProviderUtils::effective() noexcept {
    return g_effectiveProvider.load(std::memory_order_acquire);
}

void ExecutionProviderUtils::setEffective(const ExecutionProvider provider) noexcept {
    g_effectiveProvider.store(provider, std::memory_order_release);
}
