#include "ColorUtils.h"

#include <algorithm>
#include <cmath>
#include <numbers>

static double srgbToLinear(double c) {
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

static double linearToSrgb(double c) {
    return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

ColorUtils::OkLab ColorUtils::srgbToOkLab(const QColor &color) {
    double r = srgbToLinear(color.redF());
    double g = srgbToLinear(color.greenF());
    double b = srgbToLinear(color.blueF());

    double l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b;
    double m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b;
    double s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b;

    double l_ = std::cbrt(l);
    double m_ = std::cbrt(m);
    double s_ = std::cbrt(s);

    return {
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_
    };
}

QColor ColorUtils::oklabToSRGB(const OkLab &lab) {
    double l_ = lab.L + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
    double m_ = lab.L - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
    double s_ = lab.L - 0.0894841775 * lab.a - 1.2914855480 * lab.b;

    double l = l_ * l_ * l_;
    double m = m_ * m_ * m_;
    double s = s_ * s_ * s_;

    double r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    double b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

    return QColor::fromRgbF(std::clamp(linearToSrgb(r), 0.0, 1.0),
                            std::clamp(linearToSrgb(g), 0.0, 1.0),
                            std::clamp(linearToSrgb(b), 0.0, 1.0));
}

static void oklabToLinearRGB(const ColorUtils::OkLab &lab, double &r, double &g, double &b) {
    double l_ = lab.L + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
    double m_ = lab.L - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
    double s_ = lab.L - 0.0894841775 * lab.a - 1.2914855480 * lab.b;

    double l = l_ * l_ * l_;
    double m = m_ * m_ * m_;
    double s = s_ * s_ * s_;

    r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
}

bool ColorUtils::isInSRGBGamut(double r, double g, double b) {
    constexpr double eps = 1e-4;
    return r >= -eps && r <= 1.0 + eps &&
           g >= -eps && g <= 1.0 + eps &&
           b >= -eps && b <= 1.0 + eps;
}

QColor ColorUtils::oklchToSRGBGamutMap(const OkLCH &lch) {
    double r, g, b;
    auto lab = oklchToOkLab(lch);
    oklabToLinearRGB(lab, r, g, b);

    if (isInSRGBGamut(r, g, b)) {
        return QColor::fromRgbF(std::clamp(linearToSrgb(r), 0.0, 1.0),
                                std::clamp(linearToSrgb(g), 0.0, 1.0),
                                std::clamp(linearToSrgb(b), 0.0, 1.0));
    }

    double lo = 0.0;
    double hi = lch.C;
    constexpr int maxIter = 20;
    constexpr double chromaEps = 1e-4;

    for (int i = 0; i < maxIter; i++) {
        double mid = (lo + hi) * 0.5;
        if (hi - lo < chromaEps)
            break;

        auto testLab = oklchToOkLab({lch.L, mid, lch.H});
        oklabToLinearRGB(testLab, r, g, b);

        if (isInSRGBGamut(r, g, b))
            lo = mid;
        else
            hi = mid;
    }

    auto finalLab = oklchToOkLab({lch.L, lo, lch.H});
    oklabToLinearRGB(finalLab, r, g, b);
    return QColor::fromRgbF(std::clamp(linearToSrgb(r), 0.0, 1.0),
                            std::clamp(linearToSrgb(g), 0.0, 1.0),
                            std::clamp(linearToSrgb(b), 0.0, 1.0));
}

ColorUtils::OkLCH ColorUtils::oklabToOkLCH(const OkLab &lab) {
    double C = std::sqrt(lab.a * lab.a + lab.b * lab.b);
    double H = std::atan2(lab.b, lab.a) * 180.0 / std::numbers::pi;
    if (H < 0)
        H += 360.0;
    return {lab.L, C, H};
}

ColorUtils::OkLab ColorUtils::oklchToOkLab(const OkLCH &lch) {
    double hRad = lch.H * std::numbers::pi / 180.0;
    return {lch.L, lch.C * std::cos(hRad), lch.C * std::sin(hRad)};
}

ColorUtils::OkLCH ColorUtils::srgbToOkLCH(const QColor &color) {
    return oklabToOkLCH(srgbToOkLab(color));
}

QColor ColorUtils::oklchToSRGB(const OkLCH &lch) {
    return oklchToSRGBGamutMap(lch);
}

QColor ColorUtils::adjustLightness(const QColor &base, double deltaL) {
    auto lch = srgbToOkLCH(base);
    lch.L = std::clamp(lch.L + deltaL, 0.0, 1.0);
    auto result = oklchToSRGB(lch);
    result.setAlpha(base.alpha());
    return result;
}

QColor ColorUtils::adjustAlpha(const QColor &base, int alpha) {
    QColor result = base;
    result.setAlpha(std::clamp(alpha, 0, 255));
    return result;
}
