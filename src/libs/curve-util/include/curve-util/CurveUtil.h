#pragma once

#include <filesystem>
#include <vector>

#include <curve-util/CurveUtilGlobal.h>

namespace CurveUtil
{
    double CURVE_UTIL_EXPORT tickToms(double tick, double tempo);
    double CURVE_UTIL_EXPORT linearInterpolation(double x1, double y1, double x2, double y2, double x);
    std::pair<int, std::vector<double>>
        CURVE_UTIL_EXPORT alignCurve(double index, int step, const std::vector<double> &values, double interval);
    std::vector<double> CURVE_UTIL_EXPORT alignCurve(double offset, const std::vector<double> &values, double interval);


    class CURVE_UTIL_EXPORT SinusoidalSmoothingConv1d {
    public:
        explicit SinusoidalSmoothingConv1d(int kernel_size);
        std::vector<double> forward(const std::vector<double> &x) const;

    private:
        int kernel_size;
        std::vector<double> kernel;
    };
} // namespace CurveUtil
