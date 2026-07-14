#ifndef SINGERMODELSESSION_H
#define SINGERMODELSESSION_H

#include <mutex>

#include <diffsinger/Infer/ModelSet.h>
#include <synthrt/Core/Support/Expected.h>
#include <synthrt/SVS/Inference.h>
#include <synthrt/SVS/InferenceContrib.h>

#include "Modules/Inference/Models/SingerIdentifier.h"

class SingerModelSession final {
public:
    struct Model {
        srt::core::NO<srt::svs::Inference> inference;
        srt::core::NO<srt::svs::InferenceImportOptions> importOptions;
    };

    SingerModelSession(SingerIdentifier identifier, ds::infer::ModelSet modelSet);
    ~SingerModelSession();

    SingerModelSession(const SingerModelSession &) = delete;
    SingerModelSession &operator=(const SingerModelSession &) = delete;

    const SingerIdentifier &identifier() const noexcept;
    srt::core::Expected<Model> acquire(ds::infer::StageKind kind);

private:
    SingerIdentifier m_identifier;
    ds::infer::ModelSet m_modelSet;
    std::mutex m_modelSetMutex;
};

#endif // SINGERMODELSESSION_H
