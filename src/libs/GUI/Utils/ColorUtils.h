#ifndef COLORUTILS_H
#define COLORUTILS_H

#include <QColor>

class ColorUtils {
public:
    struct OkLab {
        double L;
        double a;
        double b;
    };

    struct OkLCH {
        double L;
        double C;
        double H;
    };

    static OkLab srgbToOkLab(const QColor &color);
    static QColor oklabToSRGB(const OkLab &lab);

    static OkLCH oklabToOkLCH(const OkLab &lab);
    static OkLab oklchToOkLab(const OkLCH &lch);

    static OkLCH srgbToOkLCH(const QColor &color);
    static QColor oklchToSRGB(const OkLCH &lch);

    static QColor adjustLightness(const QColor &base, double deltaL);
    static QColor adjustAlpha(const QColor &base, int alpha);

private:
    static bool isInSRGBGamut(double r, double g, double b);
    static QColor oklchToSRGBGamutMap(const OkLCH &lch);
};

#endif // COLORUTILS_H
