#ifndef EXECUTIONBACKEND_H
#define EXECUTIONBACKEND_H

#include <optional>
#include <string_view>

namespace lite::synthrt {

    /// Execution provider names stored in editor settings.
    enum class Backend {
        Cpu,
        DirectML,
        Cuda,
        CoreML,
    };

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

    constexpr std::optional<Backend> parseBackendName(std::string_view name) noexcept {
        for (const auto backend : {Backend::Cpu, Backend::DirectML, Backend::Cuda, Backend::CoreML}) {
            if (backendName(backend) == name) {
                return backend;
            }
        }
        return std::nullopt;
    }

    /// Unknown provider names fall back to CPU.
    constexpr Backend backendFromName(std::string_view name) noexcept {
        return parseBackendName(name).value_or(Backend::Cpu);
    }

}

#endif // EXECUTIONBACKEND_H
