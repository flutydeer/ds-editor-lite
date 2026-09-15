#ifndef EXECUTIONPROVIDER_H
#define EXECUTIONPROVIDER_H

#include <QString>

#include <optional>

// Single source of truth for the ONNX Runtime execution provider: how it is
// spelled in appConfig.json, whether this build ships it, whether this machine
// can run it, and which provider the inference engine actually ended up using.
enum class ExecutionProvider { Cpu, DirectML, Cuda };

namespace ExecutionProviderUtils {
    // Persistence spelling: "CPU" / "DirectML" / "CUDA".
    QString toString(ExecutionProvider provider) noexcept;
    std::optional<ExecutionProvider> fromString(const QString &value) noexcept;

    // True when this binary was compiled with the provider at all.
    bool availableInBuild(ExecutionProvider provider) noexcept;

    // True when the provider cannot run without an enumerated GPU.
    bool requiresGpu(ExecutionProvider provider) noexcept;

    // Device facts gathered by the caller; the real probing (DXGI / nvidia-smi)
    // lives in InferEngine and InferencePage so this module stays dependency-free
    // and unit-testable.
    struct DeviceAvailability {
        bool gpuFound = false;
    };

    struct Resolution {
        ExecutionProvider provider = ExecutionProvider::Cpu;
        bool changed = false;
        // Empty when unchanged. English, log-only: the GUI composes its own
        // localized message from the previous provider name.
        QString reason;
    };

    // Persisted value + real device facts -> the provider that will actually run.
    Resolution resolve(const QString &persisted, const DeviceAvailability &devices);

    // Process-wide effective provider. Written once by InferEngine before the
    // synthrt runtime starts and read from worker threads by the DirectML
    // serialization guard, so it must never touch singletons.
    ExecutionProvider effective() noexcept;
    void setEffective(ExecutionProvider provider) noexcept;
} // namespace ExecutionProviderUtils

#endif // EXECUTIONPROVIDER_H
