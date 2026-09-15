#include "Modules/Inference/ExecutionProvider.h"

#include <QCoreApplication>
#include <QDebug>

namespace {

    using ExecutionProviderUtils::DeviceAvailability;

    bool expect(const bool condition, const QString &message) {
        if (condition) {
            return true;
        }
        qCritical().noquote() << message;
        return false;
    }

    bool testResolve(const bool cudaInBuild) {
        bool success = true;
        const DeviceAvailability noGpu{};       // no adapter passed the usability filter
        const DeviceAvailability withGpu{true}; // at least one usable adapter

        {
            const auto result = ExecutionProviderUtils::resolve(QStringLiteral("CPU"), noGpu);
            success &= expect(result.provider == ExecutionProvider::Cpu && !result.changed,
                              QStringLiteral("CPU must be preserved"));
        }
#if defined(Q_OS_WIN)
        {
            const auto result =
                ExecutionProviderUtils::resolve(QStringLiteral("DirectML"), withGpu);
            success &= expect(result.provider == ExecutionProvider::DirectML && !result.changed,
                              QStringLiteral("DirectML with a usable device must be preserved"));
        }
        {
            const auto result = ExecutionProviderUtils::resolve(QStringLiteral("DirectML"), noGpu);
            success &= expect(result.provider == ExecutionProvider::Cpu && result.changed,
                              QStringLiteral("DirectML without a device must fall back to CPU"));
        }
#else
        {
            const auto result =
                ExecutionProviderUtils::resolve(QStringLiteral("DirectML"), withGpu);
            success &= expect(result.provider == ExecutionProvider::Cpu && result.changed,
                              QStringLiteral("DirectML must fall back to CPU outside Windows"));
        }
#endif
        if (cudaInBuild) {
            const auto withDevice =
                ExecutionProviderUtils::resolve(QStringLiteral("CUDA"), withGpu);
            success &= expect(withDevice.provider == ExecutionProvider::Cuda && !withDevice.changed,
                              QStringLiteral("CUDA with a usable device must be preserved"));
            const auto noDevice = ExecutionProviderUtils::resolve(QStringLiteral("CUDA"), noGpu);
            success &= expect(noDevice.provider == ExecutionProvider::Cpu && noDevice.changed,
                              QStringLiteral("CUDA without a device must fall back to CPU"));
        } else {
            const auto result = ExecutionProviderUtils::resolve(QStringLiteral("CUDA"), withGpu);
            success &= expect(result.provider == ExecutionProvider::Cpu && result.changed,
                              QStringLiteral("CUDA must fall back to CPU in a non-CUDA build"));
        }

        const QStringList unknownProviders{QStringLiteral("BogusEP"), QString()};
        for (const auto &raw : unknownProviders) {
            const auto result = ExecutionProviderUtils::resolve(raw, withGpu);
            success &= expect(result.provider == ExecutionProvider::Cpu && result.changed,
                              QStringLiteral("an unknown provider must fall back to CPU"));
        }

        const QList<ExecutionProvider> allProviders{
            ExecutionProvider::Cpu, ExecutionProvider::DirectML, ExecutionProvider::Cuda};
        for (const auto provider : allProviders) {
            const auto parsed =
                ExecutionProviderUtils::fromString(ExecutionProviderUtils::toString(provider));
            success &= expect(parsed.has_value() && *parsed == provider,
                              QStringLiteral("the provider codec must round-trip"));
        }
        success &= expect(!ExecutionProviderUtils::fromString(QStringLiteral("gpu")).has_value(),
                          QStringLiteral("an unknown provider string must not decode"));

        ExecutionProviderUtils::setEffective(ExecutionProvider::DirectML);
        success &=
            expect(ExecutionProviderUtils::effective() == ExecutionProvider::DirectML,
                   QStringLiteral("the effective provider must be readable after being set"));
        ExecutionProviderUtils::setEffective(ExecutionProvider::Cpu);
        success &= expect(ExecutionProviderUtils::effective() == ExecutionProvider::Cpu,
                          QStringLiteral("the effective provider must go back to CPU"));
        return success;
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
#if defined(ONNXRUNTIME_ENABLE_CUDA)
    return testResolve(true) ? 0 : 1;
#else
    return testResolve(false) ? 0 : 1;
#endif
}
