#include "UI/Views/TrackEditor/ClipResizeUtils.h"
#include "UI/Views/TrackEditor/SingingClipPreviewLayout.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cmath>

namespace {
    int g_failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }

    bool closeTo(const double left, const double right) {
        return std::abs(left - right) < 0.0001;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    Clip::ClipCommonProperties singing;
    singing.length = 1920;
    singing.clipStart = 120;
    singing.clipLen = 960;
    expect(ClipResizeUtils::updateRightEdge(singing, 3000, true, 1800),
           "a positive singing clip resize must be accepted");
    expect(singing.clipLen == 3000 && singing.length == 3120,
           "expanding a singing clip must extend its editable content length");
    expect(ClipResizeUtils::updateRightEdge(singing, 600, true, 1800),
           "shrinking a singing clip must be accepted");
    expect(singing.clipLen == 600 && singing.length == 1800,
           "shrinking must retain enough content length for existing notes");

    Clip::ClipCommonProperties audio;
    audio.length = 2000;
    audio.clipStart = 500;
    audio.clipLen = 500;
    expect(ClipResizeUtils::updateRightEdge(audio, 3000, false, 2000),
           "a positive audio clip resize must be accepted");
    expect(audio.clipLen == 1500 && audio.length == 2000,
           "audio resize must stop at the material boundary");
    expect(!ClipResizeUtils::updateRightEdge(audio, 0, false, 2000),
           "a non-positive visible length must be rejected");

    const QRectF preview(10.0, 20.0, 200.0, 80.0);
    const auto layout = SingingClipPreview::computeLayout(preview, {60, 64, 67});
    expect(layout.valid(), "a note range must produce a valid preview layout");
    expect(layout.lowestKeyIndex == 60 && layout.highestKeyIndex == 67,
           "preview layout must preserve the note range");
    expect(closeTo(layout.noteHeight, 8.0),
           "preview notes must use the classic backend's maximum height");
    expect(closeTo(layout.contentTop, 28.0),
           "a compact note range must be vertically centered in the preview");
    expect(closeTo(layout.keyIndexAt(layout.contentTop), 67.0),
           "the preview inverse mapping must return the highest key at the content top");
    expect(closeTo(layout.keyIndexAt(layout.contentTop + 7.0 * layout.noteHeight), 60.0),
           "the preview inverse mapping must return the lowest key at the last row");

    const auto physicalLayout =
        SingingClipPreview::computeLayout(QRectF(20.0, 40.0, 400.0, 160.0), {60, 67}, 16.0);
    expect(closeTo(physicalLayout.noteHeight, 16.0) && closeTo(physicalLayout.contentTop, 56.0),
           "device-pixel scaling must preserve the logical preview layout");
    expect(!SingingClipPreview::computeLayout(preview, {}).valid(),
           "an empty note range must not produce a preview layout");

    if (g_failures == 0) {
        QTextStream(stdout) << "All TrackEditorInteractions tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
