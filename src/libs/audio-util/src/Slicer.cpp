#include <audio-util/Slicer.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace AudioUtil
{
    // https://github.com/stakira/OpenUtau/blob/master/OpenUtau.Core/Analysis/Some.cs
    Slicer::Slicer(int sampleRate, float threshold, int hopSize, int winSize, int minLength, int minInterval,
                   int maxSilKept) :
        sample_rate(sampleRate), threshold(threshold), hop_size(hopSize), win_size(winSize), min_length(minLength),
        min_interval(minInterval), max_sil_kept(maxSilKept) {}

    std::vector<double> Slicer::get_rms(const std::vector<float> &samples, const int frame_length,
                                        const int hop_length) {
        std::vector<float> y(samples.size() + frame_length, 0);
        std::copy(samples.begin(), samples.end(), y.begin() + frame_length / 2);

        std::vector<double> output;
        const size_t output_size = samples.size() / hop_length;
        output.reserve(output_size);

        for (size_t i = 0; i < output_size; ++i) {
            const int start = static_cast<int>(i * hop_length);
            const int end = start + frame_length;
            const double sum = std::accumulate(y.begin() + start, y.begin() + end, 0.0,
                                               [](const double acc, const float value) { return acc + value * value; });
            output.push_back(std::sqrt(sum / frame_length));
        }

        return output;
    }

    int Slicer::argmin(const std::vector<double> &array) {
        return std::distance(array.begin(), std::min_element(array.begin(), array.end()));
    }

    MarkerList Slicer::slice(const std::vector<float> &samples) const {
        if ((samples.size() + hop_size - 1) / hop_size <= min_length) {
            return {{0, samples.size()}};
        }

        auto rms_list = get_rms(samples, win_size, hop_size);
        std::vector<std::tuple<int, int>> sil_tags;
        int silence_start = -1;
        int clip_start = 0;

        for (size_t i = 0; i < rms_list.size(); ++i) {
            const double rms = rms_list[i];

            if (rms < threshold) {
                if (silence_start < 0) {
                    silence_start = static_cast<int>(i);
                }
                continue;
            }

            if (silence_start < 0) {
                continue;
            }

            bool is_leading_silence = silence_start == 0 && i > max_sil_kept;
            bool need_slice_middle = i - silence_start >= min_interval && i - clip_start >= min_length;

            if (!is_leading_silence && !need_slice_middle) {
                silence_start = -1;
                continue;
            }

            if (i - silence_start <= max_sil_kept) {
                int pos = argmin({rms_list.begin() + silence_start, rms_list.begin() + i + 1});
                pos += silence_start;
                sil_tags.emplace_back((silence_start == 0 ? 0 : pos), pos);
                clip_start = pos;
            } else {
                int pos_l =
                    argmin({rms_list.begin() + silence_start, rms_list.begin() + silence_start + max_sil_kept + 1});
                int pos_r = argmin({rms_list.begin() + i - max_sil_kept, rms_list.begin() + i + 1});
                pos_l += silence_start;
                pos_r += static_cast<int>(i - max_sil_kept);

                if (silence_start == 0) {
                    sil_tags.emplace_back(0, pos_r);
                } else {
                    sil_tags.emplace_back(pos_l, pos_r);
                }

                clip_start = pos_r;
            }

            silence_start = -1;
        }

        if (silence_start >= 0 && rms_list.size() - silence_start >= min_interval) {
            const int silence_end = std::min<int>(static_cast<int>(rms_list.size()), silence_start + max_sil_kept);
            int pos = argmin({rms_list.begin() + silence_start, rms_list.begin() + silence_end + 1});
            pos += silence_start;
            sil_tags.emplace_back(pos, rms_list.size() + 1);
        }

        if (sil_tags.empty()) {
            return {{0, samples.size()}};
        } else {
            MarkerList chunks;

            if (std::get<0>(sil_tags[0]) > 0) {
                chunks.emplace_back(0, std::get<0>(sil_tags[0]) * hop_size);
            }

            for (size_t i = 0; i < sil_tags.size() - 1; ++i) {
                chunks.emplace_back(std::get<1>(sil_tags[i]) * hop_size, std::get<0>(sil_tags[i + 1]) * hop_size);
            }

            if (std::get<1>(sil_tags.back()) < rms_list.size()) {
                chunks.emplace_back(std::get<1>(sil_tags.back()) * hop_size, rms_list.size() * hop_size);
            }
            return chunks;
        }
    }
} // namespace AudioUtil
