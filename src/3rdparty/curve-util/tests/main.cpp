#include <curve-util/CurveUtil.h>
#include <iostream>
#include <vector>

using namespace CurveUtil;
int main() {
    const double result = linearInterpolation(1.0, 1.0, 2.6, 5.0, 1.8);
    std::cout << "线性插值结果: " << result << std::endl; // Expected result: 3.0.

    const std::vector<double> values = {1.0, 3.0, 5.0, 7.0};
    double offset = 0.25;
    constexpr double interval = 1.0;
    const std::vector<double> alignedValues = alignCurve(offset, values, interval);

    std::cout << "对齐后的曲线数据: ";
    for (const auto &val : alignedValues) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    offset = -0.5;
    const std::vector<double> alignedValues2 = alignCurve(offset, values, interval);
    std::cout << "对齐后的曲线数据 (offset = -0.5): ";
    for (const auto &val : alignedValues2) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Offsets with |offset| >= interval report an error and return an empty curve.
    offset = 2.0;
    const std::vector<double> alignedValues3 = alignCurve(offset, values, interval);
    std::cout << "对齐后的曲线数据 (offset = 2.0): ";

    return 0;
}
