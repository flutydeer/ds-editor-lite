#ifndef ANALYZERREFERENCE_H
#define ANALYZERREFERENCE_H

#include <optional>
#include <string>
#include <string_view>

#include <synthrt/Core/ContribLocator.h>
#include <synthrt/SVS/InferenceContrib.h>

namespace lite::synthrt {

    /// Versionless analyzer reference: package:inference/contribution.
    /// The engine resolves the highest loaded package version.
    struct AnalyzerReference {
        std::string packageId;
        std::string contributionId;

        std::string toString() const {
            return ::srt::ContribLocator(packageId, ::srt::InferenceCategory::NAME, contributionId)
                .toString();
        }

        /// Parse an inference reference with an explicit package, or return no value.
        static std::optional<AnalyzerReference> parse(std::string_view text) {
            return parse(text, ::srt::InferenceCategory::NAME);
        }

        /// Normalize package:analysis/contribution to the inference category; leave other text
        /// unchanged.
        static std::string upgrade(std::string_view text) {
            if (auto legacy = parse(text, "analysis")) {
                return legacy->toString();
            }
            return std::string(text);
        }

    private:
        static std::optional<AnalyzerReference> parse(std::string_view text,
                                                      std::string_view category) {
            const auto locator = ::srt::ContribLocator::fromString(text);
            // Stored settings require an explicit package because they have no containing package.
            if (!locator.isValid() || locator.isLocal() || locator.category() != category) {
                return std::nullopt;
            }
            return AnalyzerReference{locator.packageId(), locator.contributionId()};
        }
    };

}

#endif // ANALYZERREFERENCE_H
