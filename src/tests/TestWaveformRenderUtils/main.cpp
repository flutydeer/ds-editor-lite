#include "UI/Utils/WaveformRenderUtils.h"

#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QTextStream>
#include <QTransform>

#include <cmath>

namespace {
    int g_failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }

    int coloredPixelCount(const QImage &image) {
        auto result = 0;
        for (auto y = 0; y < image.height(); ++y) {
            for (auto x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 0)
                    ++result;
            }
        }
        return result;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QImage image(128, 64, QImage::Format_ARGB32_Premultiplied);
    const QColor color(80, 140, 255);

    const auto logarithmicQuarter =
        WaveformRenderUtils::mapAmplitude(0.25, WaveformRenderUtils::AmplitudeScale::Logarithmic);
    const auto expectedLogarithmicQuarter = std::log(1.0 + 15.0 * 0.25) / std::log(16.0);
    expect(std::abs(WaveformRenderUtils::mapAmplitude(0.25,
                                                      WaveformRenderUtils::AmplitudeScale::Linear) -
                    0.25) < 1e-12,
           "linear amplitude mapping must preserve accompaniment levels");
    expect(std::abs(logarithmicQuarter - expectedLogarithmicQuarter) < 1e-12,
           "logarithmic amplitude mapping must preserve the phoneme visual scale");
    expect(std::abs(WaveformRenderUtils::mapAmplitude(
                        -0.25, WaveformRenderUtils::AmplitudeScale::Logarithmic) +
                    logarithmicQuarter) < 1e-12,
           "logarithmic amplitude mapping must remain symmetric");
    expect(std::abs(WaveformRenderUtils::mapAmplitude(
                        1.0, WaveformRenderUtils::AmplitudeScale::Logarithmic) -
                    1.0) < 1e-12,
           "logarithmic amplitude mapping must preserve full scale");

    WaveformRenderUtils::SampledWaveform waveform;
    waveform.geometry = WaveformRenderUtils::Geometry::FilledPeaks;
    waveform.peaks = {
        {8.0, 48.0, 16.0},
        {64.0, 52.0, 12.0},
        {120.0, 48.0, 16.0},
    };
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        WaveformRenderUtils::renderWaveform(&painter, color, WaveformRenderUtils::FilledMode,
                                            waveform);
    }
    expect(coloredPixelCount(image) > 1000,
           "filled peak geometry must render through the shared backend interface");

    waveform.geometry = WaveformRenderUtils::Geometry::VerticalPeaks;
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        WaveformRenderUtils::renderWaveform(&painter, color, WaveformRenderUtils::FilledMode,
                                            waveform);
    }
    expect(coloredPixelCount(image) > 50,
           "sub-chunk peak geometry must retain vertical-line rendering");

    waveform = {};
    waveform.geometry = WaveformRenderUtils::Geometry::Curve;
    waveform.curve = {{8.0, 32.0}, {40.0, 12.0}, {72.0, 52.0}, {120.0, 24.0}};
    waveform.sampleDots = {{40.0, 12.0}, {72.0, 52.0}};
    waveform.sampleDotRadius = 2.0;
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        WaveformRenderUtils::renderWaveform(&painter, color, WaveformRenderUtils::FilledMode,
                                            waveform);
    }
    expect(coloredPixelCount(image) > 100,
           "high-zoom curve and sample dots must render through the shared backend interface");

    waveform.curve = {{108.0, 32.0}, {140.0, 12.0}, {172.0, 52.0}, {220.0, 24.0}};
    waveform.sampleDots = {{140.0, 12.0}, {172.0, 52.0}};
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        WaveformRenderUtils::renderWaveform(&painter, color, WaveformRenderUtils::FilledMode,
                                            waveform, QTransform::fromTranslate(-100.0, 0.0));
    }
    expect(coloredPixelCount(image) > 100,
           "legacy graphics items must be able to map backend-neutral waveform geometry");

    if (g_failures == 0) {
        QTextStream(stdout) << "All WaveformRenderUtils tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
