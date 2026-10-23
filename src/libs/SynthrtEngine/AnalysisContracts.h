#ifndef ANALYSISCONTRACTS_H
#define ANALYSISCONTRACTS_H

#include <memory>
#include <string_view>

#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/Support/Expected.h>

#include <otter/Analysis/AnalysisExecutive.h>

namespace lite::synthrt {

    /// Supported otter analysis contracts and their factories.
    struct AnalysisContract {
        std::string_view interfaceName;
        ::srt::Expected<std::unique_ptr<otter::AnalysisExecutive>> (*create)(
            ::srt::InferenceSpec &spec);
    };

    const AnalysisContract *findAnalysisContract(std::string_view interfaceName);

}

#endif // ANALYSISCONTRACTS_H
