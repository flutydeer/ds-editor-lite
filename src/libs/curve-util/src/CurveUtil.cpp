#include <curve-util/CurveUtil.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace CurveUtil
{
    static int round(const int tick, const int step) {
        const int times = tick / step;
        const int mod = tick % step;
        if (mod > step / 2)
            return step * (times + 1);
        return step * times;
    }

    double tickToms(const double tick, const double tempo) { return tick * 8 * tempo; }

    double linearInterpolation(const double x1, const double y1, const double x2, const double y2, const double x) {
        return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
    }

    std::vector<double> alignCurve(const double offset, const std::vector<double> &values, const double interval) {
        std::vector<double> alignValues;
        if (offset == 0)
            return values;

        if (abs(offset) >= interval) {
            std::cerr << "CurveUtil Error: offset too large." << std::endl;
            return alignValues;
        }

        if (values.size() < 2 || interval <= 0) {
            std::cerr << "CurveUtil Error: values.size() < 2 or interval <= 0." << std::endl;
            return alignValues;
        }

        if (offset > 0) {
            for (int i = 0; i < values.size() - 1; i++)
                alignValues.push_back(linearInterpolation(i * interval, values[i], (i + 1) * interval, values[i + 1],
                                                          i * interval + offset));
            alignValues.push_back(values.back());
        } else {
            alignValues.push_back(values.front());
            for (int i = 1; i < values.size(); i++)
                alignValues.push_back(linearInterpolation((i - 1) * interval, values[i - 1], i * interval, values[i],
                                                          i * interval + offset));
        }
        return alignValues;
    }

    std::pair<int, std::vector<double>> alignCurve(const double index, const int step,
                                                   const std::vector<double> &values, const double interval) {
        const auto roundIndex = round(static_cast<int>(index), step);
        return {roundIndex, alignCurve(index - roundIndex, values, interval)};
    }

    SinusoidalSmoothingConv1d::SinusoidalSmoothingConv1d(const int kernel_size) :
        kernel_size(std::max(kernel_size, 1)) {
        if (this->kernel_size > 1) {
            kernel.resize(this->kernel_size);
            float kernel_sum = 0.0f;

            // Precompute step value to avoid repeated division
            const float step = 1.0f / (this->kernel_size - 1);

            for (int i = 0; i < this->kernel_size; ++i) {
                constexpr float PI = 3.14159265358979323846f;
                const float t_val = i * step;
                kernel[i] = std::sin(PI * t_val);
                kernel_sum += kernel[i];
            }

            // Normalize kernel in a single pass
            const float inv_kernel_sum = 1.0f / kernel_sum;
            std::transform(kernel.begin(), kernel.end(), kernel.begin(),
                           [inv_kernel_sum](const float val) { return val * inv_kernel_sum; });
        } else {
            kernel = {1.0f};
        }
    }

    std::vector<double> SinusoidalSmoothingConv1d::forward(const std::vector<double> &x) const {
        if (kernel_size == 1)
            return x;
        if (x.empty())
            return {};

        const int K = kernel_size;
        const int L = x.size();
        const int total_pad = K - 1;
        const int left_pad = total_pad / 2;
        const int right_pad = total_pad - left_pad;

        std::vector<double> padded_x;
        padded_x.reserve(L + total_pad);

        for (int i = 0; i < left_pad; ++i) {
            padded_x.push_back(x[0]);
        }

        padded_x.insert(padded_x.end(), x.begin(), x.end());

        for (int i = 0; i < right_pad; ++i) {
            padded_x.push_back(x.back());
        }

        std::vector<double> output;
        output.reserve(L);

        for (int i = 0; i < L; ++i) {
            double conv_sum = 0.0;
            for (int j = 0; j < K; ++j) {
                conv_sum += padded_x[i + j] * kernel[j];
            }
            output.push_back(conv_sum);
        }

        return output;
    }

} // namespace CurveUtil
