#include "AudioSlicer.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Extractors {

    namespace {

        /// Returns the offset from \a begin to the first smallest element, or 0 for an empty range.
        ///
        /// Zero rather than an assertion or a negative: the caller adds this to a start index
        /// without checking it, so anything else would index out of the list. The ranges the
        /// caller passes are non-empty in the normal flow; this is what keeps a clamped parameter
        /// from turning into a memory error.
        template <typename Iterator>
        int64_t smallest(const Iterator &begin, const Iterator &end) {
            if (begin == end) {
                return 0;
            }
            return std::distance(begin, std::min_element(begin, end));
        }

        std::vector<double> rootMeanSquares(const std::vector<float> &samples, int64_t window,
                                            int64_t hop) {
            std::vector<double> output;
            if (window <= 0 || hop <= 0) {
                return output;
            }
            const auto count = static_cast<int64_t>(samples.size()) / hop;
            output.reserve(static_cast<std::size_t>(count));

            for (int64_t i = 0; i < count; ++i) {
                const auto centre = i * hop - window / 2;
                const auto from = static_cast<std::size_t>(std::max<int64_t>(0, centre));
                const auto to = std::min(samples.size(),
                                         static_cast<std::size_t>(std::max<int64_t>(0, centre)) +
                                             static_cast<std::size_t>(window));
                const auto sum =
                    std::accumulate(samples.begin() + from, samples.begin() + to, 0.0,
                                    [](double acc, float value) { return acc + value * value; });
                output.push_back(std::sqrt(sum / static_cast<double>(window)));
            }
            return output;
        }

        /// Divides any span longer than \a limit into equal pieces.
        std::vector<SampleSpan> capped(const std::vector<SampleSpan> &spans, int64_t limit) {
            if (limit <= 0) {
                return spans;
            }
            std::vector<SampleSpan> result;
            result.reserve(spans.size());
            for (const auto &span : spans) {
                const auto length = span.length();
                if (length <= limit) {
                    result.push_back(span);
                    continue;
                }
                const auto pieces = (length + limit - 1) / limit;
                auto at = span.begin;
                for (int64_t i = 0; i < pieces; ++i) {
                    // Recomputed from the ends rather than accumulated, so rounding cannot leave a
                    // gap or an overlap between adjacent pieces.
                    const auto to = span.begin + (length * (i + 1)) / pieces;
                    result.push_back({at, to});
                    at = to;
                }
            }
            return result;
        }

        int64_t inFrames(double seconds, int sampleRate) {
            return std::max<int64_t>(0, static_cast<int64_t>(std::llround(seconds * sampleRate)));
        }

    }

    std::vector<SampleSpan> SlicingProfile::slice(const std::vector<float> &samples,
                                                  int sampleRate, double maximumSpan) const {
        const auto total = static_cast<int64_t>(samples.size());
        const SampleSpan whole{0, total};
        if (sampleRate <= 0 || total == 0) {
            return {whole};
        }

        const auto limit = inFrames(maximumSpan, sampleRate);
        const auto hopSize = inFrames(hop, sampleRate);
        const auto windowSize = inFrames(window, sampleRate);
        if (hopSize <= 0 || windowSize <= 0) {
            return capped({whole}, limit);
        }

        // The lengths the algorithm compares are counted in analysis frames, so they are converted
        // once here rather than at every comparison.
        const auto minimumFrames = inFrames(minimumLength, sampleRate) / hopSize;
        const auto intervalFrames = inFrames(minimumInterval, sampleRate) / hopSize;
        const auto silenceFrames = inFrames(maximumSilence, sampleRate) / hopSize;

        if ((total + hopSize - 1) / hopSize <= minimumFrames) {
            return capped({whole}, limit);
        }

        const auto levels = rootMeanSquares(samples, windowSize, hopSize);
        const auto frames = static_cast<int64_t>(levels.size());
        std::vector<SampleSpan> silences;
        int64_t silenceStart = -1;
        int64_t clipStart = 0;

        for (int64_t i = 0; i < frames; ++i) {
            if (levels[i] < threshold) {
                if (silenceStart < 0) {
                    silenceStart = i;
                }
                continue;
            }
            if (silenceStart < 0) {
                continue;
            }

            const bool leading = silenceStart == 0 && i > silenceFrames;
            const bool worthCutting =
                i - silenceStart >= intervalFrames && i - clipStart >= minimumFrames;
            if (!leading && !worthCutting) {
                silenceStart = -1;
                continue;
            }

            if (i - silenceStart <= silenceFrames) {
                auto at = smallest(levels.begin() + silenceStart, levels.begin() + i + 1);
                at += silenceStart;
                silences.push_back({silenceStart == 0 ? 0 : at, at});
                clipStart = at;
            } else {
                auto left = smallest(levels.begin() + silenceStart,
                                     levels.begin() + silenceStart + silenceFrames + 1);
                auto right = smallest(levels.begin() + i - silenceFrames, levels.begin() + i + 1);
                left += silenceStart;
                right += i - silenceFrames;
                silences.push_back({silenceStart == 0 ? 0 : left, right});
                clipStart = right;
            }
            silenceStart = -1;
        }

        if (silenceStart >= 0 && frames - silenceStart >= intervalFrames) {
            const auto until = std::min(frames - 1, silenceStart + silenceFrames);
            auto at = smallest(levels.begin() + silenceStart, levels.begin() + until + 1);
            at += silenceStart;
            silences.push_back({at, frames + 1});
        }

        if (silences.empty()) {
            return capped({whole}, limit);
        }

        std::vector<SampleSpan> spans;
        if (silences.front().begin > 0) {
            spans.push_back({0, silences.front().begin * hopSize});
        }
        for (std::size_t i = 0; i + 1 < silences.size(); ++i) {
            spans.push_back({silences[i].end * hopSize, silences[i + 1].begin * hopSize});
        }
        if (silences.back().end < frames) {
            spans.push_back({silences.back().end * hopSize, frames * hopSize});
        }
        if (spans.empty()) {
            return capped({whole}, limit);
        }
        // The frame grid never reaches past the last whole hop, so the tail is restored here
        // rather than silently dropped along with up to one hop of audio.
        if (spans.back().end > total) {
            spans.back().end = total;
        }
        return capped(spans, limit);
    }

}
