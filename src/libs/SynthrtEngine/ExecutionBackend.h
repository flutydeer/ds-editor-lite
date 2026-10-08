#ifndef EXECUTIONBACKEND_H
#define EXECUTIONBACKEND_H

#include <optional>
#include <string_view>

namespace lite::synthrt {

    /// ONNX Runtime execution provider on which models run.
    ///
    /// This is the only enumeration of execution providers in the editor, which the application
    /// refers to as ExecutionProvider. backendName() and parseBackendName() define the only
    /// spelling of each provider, which the settings store. The providers that a build offers are
    /// determined by ExecutionProviderUtils::availableInBuild(). This header has no source file
    /// and no synthrt dependency, so that code which only stores a provider does not depend on the
    /// engine.
    enum class Backend {
        Cpu,
        DirectML,
        Cuda,
        CoreML,
    };

    /// Returns the name under which the settings store \a backend: "CPU", "DirectML", "CUDA" or
    /// "CoreML".
    constexpr std::string_view backendName(Backend backend) noexcept {
        switch (backend) {
            case Backend::DirectML:
                return "DirectML";
            case Backend::Cuda:
                return "CUDA";
            case Backend::CoreML:
                return "CoreML";
            case Backend::Cpu:
                break;
        }
        return "CPU";
    }

    /// Parses a backend name as stored in the editor's settings. Returns the backend, or
    /// \c std::nullopt if \a name is not a value of backendName().
    constexpr std::optional<Backend> parseBackendName(std::string_view name) noexcept {
        for (const auto backend : {Backend::Cpu, Backend::DirectML, Backend::Cuda, Backend::CoreML}) {
            if (backendName(backend) == name) {
                return backend;
            }
        }
        return std::nullopt;
    }

    /// Parses a backend name as stored in the editor's settings.
    ///
    /// Returns Cpu for an unknown name rather than failing, so that a settings file written by a
    /// newer build or on another platform does not prevent the editor from starting.
    constexpr Backend backendFromName(std::string_view name) noexcept {
        return parseBackendName(name).value_or(Backend::Cpu);
    }

}

#endif // EXECUTIONBACKEND_H
