#ifndef ANALYZERREFERENCE_H
#define ANALYZERREFERENCE_H

#include <optional>
#include <string>
#include <string_view>

#include <synthrt/SVS/InferenceContrib.h>

namespace lite::synthrt {

    /// The reference that the editor stores for an analyser,
    /// <tt>package:inference/contribution</tt>.
    ///
    /// Formatting, parsing and the upgrade of older references are defined together so that they
    /// remain consistent. The category is synthrt's inference category name. The type is
    /// header-only because the settings model reads references before any engine exists.
    struct AnalyzerReference {
        std::string packageId;
        std::string contributionId;

        std::string toString() const {
            return packageId + ":" + ::srt::InferenceCategory::NAME + "/" + contributionId;
        }

        /// Parses a stored reference. Returns \c std::nullopt if \a text is not a valid reference.
        static std::optional<AnalyzerReference> parse(std::string_view text) {
            return parse(text, ::srt::InferenceCategory::NAME);
        }

        /// Rewrites a reference stored while analysers had a dedicated category.
        ///
        /// Such a reference has the form <tt>package:analysis/contribution</tt>. The package and
        /// the contribution are unchanged, so the rewrite preserves the user's selection. Returns
        /// any other text unchanged.
        static std::string upgrade(std::string_view text) {
            if (auto legacy = parse(text, "analysis")) {
                return legacy->toString();
            }
            return std::string(text);
        }

    private:
        static std::optional<AnalyzerReference> parse(std::string_view text,
                                                      std::string_view category) {
            const auto separator = text.find(':');
            if (separator == std::string_view::npos || separator == 0) {
                return std::nullopt;
            }
            const auto rest = text.substr(separator + 1);
            const auto slash = rest.find('/');
            if (slash == std::string_view::npos || rest.substr(0, slash) != category ||
                slash + 1 == rest.size()) {
                return std::nullopt;
            }
            return AnalyzerReference{std::string(text.substr(0, separator)),
                                     std::string(rest.substr(slash + 1))};
        }
    };

}

#endif // ANALYZERREFERENCE_H
