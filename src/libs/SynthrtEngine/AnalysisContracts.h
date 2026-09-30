#ifndef ANALYSISCONTRACTS_H
#define ANALYSISCONTRACTS_H

#include <memory>
#include <string_view>

#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/Support/Expected.h>

#include <otter/Analysis/AnalysisExecutive.h>

namespace lite::synthrt {

    /// An otter analysis contract that the editor runs, with the factory of its analyzers.
    ///
    /// Listing and creation use the same table. A contract therefore cannot be listed without a
    /// factory, and a contract without an entry, such as Align, is neither listed nor created as
    /// a different contract.
    struct AnalysisContract {
        std::string_view interfaceName;
        ::srt::Expected<std::unique_ptr<otter::AnalysisExecutive>> (*create)(
            ::srt::InferenceSpec &spec);
    };

    /// Returns the entry for \a interfaceName, or null if the editor does not run that contract.
    const AnalysisContract *findAnalysisContract(std::string_view interfaceName);

}

#endif // ANALYSISCONTRACTS_H
