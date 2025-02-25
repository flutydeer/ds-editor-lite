#ifndef DECIBELLINEARIZER_H
#define DECIBELLINEARIZER_H

#include <cmath>

class DecibelLinearizer {
public:
    inline static double decibelToLinearValue(const double decibel, const double factor = -24) {
        return std::exp((decibel - factor) / -factor) - std::exp(1);
    }

    inline static double linearValueToDecibel(const double linearValue, const double factor = -24) {
        return -factor * std::log(linearValue + std::exp(1)) + factor;
    }
};

#endif // DECIBELLINEARIZER_H
