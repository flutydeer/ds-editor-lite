#ifndef ANALYZERREFERENCE_H
#define ANALYZERREFERENCE_H

#include <optional>
#include <string>
#include <string_view>

#include <synthrt/Core/ContribLocator.h>
#include <synthrt/SVS/InferenceContrib.h>

namespace lite::synthrt {

    /// Stored reference to an analyzer, of the form <tt>package:inference/contribution</tt>.
    ///
    /// The syntax is that of synthrt's ContribLocator, restricted to the inference category and to
    /// a named package, so formatting and parsing are delegated to srt::ContribLocator. The only
    /// rule defined here is the upgrade of references stored while analyzers had a dedicated
    /// category.
    ///
    /// A reference carries no package version, because a ContribLocator carries none. Settings
    /// stored by earlier builds have the same form and therefore remain valid. If several versions
    /// of the package are loaded, the engine resolves the reference to the highest version; see
    /// SynthrtEngine::createAnalyzer().
    struct AnalyzerReference {
        std::string packageId;
        std::string contributionId;

        std::string toString() const {
            return ::srt::ContribLocator(packageId, ::srt::InferenceCategory::NAME, contributionId)
                .toString();
        }

        /// Parses a stored reference. Returns the reference, or \c std::nullopt if \a text is not
        /// a valid reference.
        static std::optional<AnalyzerReference> parse(std::string_view text) {
            return parse(text, ::srt::InferenceCategory::NAME);
        }

        /// Rewrites a reference stored while analyzers had a dedicated category.
        ///
        /// Such a reference has the form <tt>package:analysis/contribution</tt>. The rewrite keeps
        /// the package and the contribution, so it preserves the user's selection. Returns the
        /// rewritten reference, or \a text unchanged if \a text is not such a reference.
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
            // A local locator refers to its containing package. A stored setting has no containing
            // package, so its locator must specify the package explicitly.
            if (!locator.isValid() || locator.isLocal() || locator.category() != category) {
                return std::nullopt;
            }
            return AnalyzerReference{locator.packageId(), locator.contributionId()};
        }
    };

}

#endif // ANALYZERREFERENCE_H
