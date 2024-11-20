#include <curve-util/CurveUtil.h>

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
} // namespace CurveUtil
