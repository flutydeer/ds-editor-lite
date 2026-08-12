#include "UI/Views/TrackEditor/AudioClipDragState.h"
#include "UI/Views/TrackEditor/ClipResizeUtils.h"
#include "UI/Views/TrackEditor/SingingClipPreviewLayout.h"
#include "Global/AppGlobal.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

#include <QCoreApplication>
#include <QTextStream>

#include <array>
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

    struct ContentSpan {
        int start;
        int length;
    };
    const std::array overlappingContent = {ContentSpan{0, 1200}, ContentSpan{800, 100}};
    const auto contentEnd = ClipResizeUtils::furthestContentEnd(
        overlappingContent.cbegin(), overlappingContent.cend(), AppGlobal::ticksPerWholeNote,
        [](const ContentSpan &span) { return span.start + span.length; });
    expect(contentEnd == 1200,
           "overlapping singing notes must retain the furthest endpoint, not the latest start");
    const std::array<ContentSpan, 0> emptyContent;
    expect(ClipResizeUtils::furthestContentEnd(
               emptyContent.cbegin(), emptyContent.cend(), AppGlobal::ticksPerWholeNote,
               [](const ContentSpan &span) { return span.start + span.length; }) ==
               AppGlobal::ticksPerWholeNote,
           "an empty singing clip must retain the default editable content length");

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

    const Timeline timeline({
        {0,    120.0},
        {4800, 60.0 },
        {9600, 150.0},
    });
    constexpr double trimStartMs = 500.0;
    constexpr double playLengthMs = 2500.0;
    constexpr double materialLengthMs = 5000.0;
    constexpr int visibleStartTick = 4700;
    constexpr int grabTick = 5100;
    const auto initialCaches = AudioClip::deriveTickCaches(
        trimStartMs, playLengthMs, materialLengthMs, visibleStartTick, timeline);
    Clip::ClipCommonProperties draggedAudio;
    draggedAudio.start = initialCaches.start;
    draggedAudio.clipStart = initialCaches.clipStart;
    draggedAudio.clipLen = initialCaches.clipLen;
    draggedAudio.length = initialCaches.length;

    auto moveState = AudioClipDragState::begin(trimStartMs, playLengthMs, materialLengthMs,
                                               visibleStartTick, grabTick, timeline);
    constexpr int cursorTick = 10500;
    const auto movedVisibleStart = moveState.visibleStartForCursor(cursorTick, timeline);
    moveState.moveTo(movedVisibleStart, draggedAudio, timeline);
    expect(draggedAudio.start + draggedAudio.clipStart == movedVisibleStart,
           "audio move must preserve the cursor's realtime grab offset across tempo changes");
    moveState.writeTruth(draggedAudio);
    expect(closeTo(draggedAudio.trimStartMs, trimStartMs) &&
               closeTo(draggedAudio.playLengthMs, playLengthMs) &&
               closeTo(draggedAudio.materialLengthMs, materialLengthMs),
           "audio move must preserve all realtime truth values");

    auto leftState = AudioClipDragState::begin(trimStartMs, playLengthMs, materialLengthMs,
                                               visibleStartTick, grabTick, timeline);
    draggedAudio.start = initialCaches.start;
    draggedAudio.clipStart = initialCaches.clipStart;
    draggedAudio.clipLen = initialCaches.clipLen;
    draggedAudio.length = initialCaches.length;
    constexpr int newLeftTick = 5200;
    expect(leftState.resizeLeftTo(newLeftTick, visibleStartTick + initialCaches.clipLen,
                                  draggedAudio, timeline),
           "audio left trim must accept an edge before the original right edge");
    leftState.writeTruth(draggedAudio);
    const double materialStartMs = timeline.tickToMs(visibleStartTick) - trimStartMs;
    const double originalEndMs = timeline.tickToMs(visibleStartTick) + playLengthMs;
    expect(closeTo(draggedAudio.trimStartMs, timeline.tickToMs(newLeftTick) - materialStartMs),
           "audio left trim must keep the material origin fixed in realtime");
    expect(closeTo(draggedAudio.playLengthMs, originalEndMs - timeline.tickToMs(newLeftTick)),
           "audio left trim must keep the original right edge fixed in realtime");

    auto rightState = AudioClipDragState::begin(trimStartMs, playLengthMs, materialLengthMs,
                                                visibleStartTick, grabTick, timeline);
    draggedAudio.start = initialCaches.start;
    draggedAudio.clipStart = initialCaches.clipStart;
    draggedAudio.clipLen = initialCaches.clipLen;
    draggedAudio.length = initialCaches.length;
    const auto beyondMaterialTick =
        qRound(timeline.msToTick(materialStartMs + materialLengthMs + 1000.0));
    expect(rightState.resizeRightTo(beyondMaterialTick, visibleStartTick, draggedAudio, timeline),
           "audio right trim must accept an edge beyond the material boundary");
    rightState.writeTruth(draggedAudio);
    expect(closeTo(draggedAudio.playLengthMs, materialLengthMs - trimStartMs),
           "audio right trim must stop at the material boundary in realtime");

    auto rejectedState = AudioClipDragState::begin(trimStartMs, playLengthMs, materialLengthMs,
                                                   visibleStartTick, grabTick, timeline);
    auto lastValidProperties = draggedAudio;
    expect(rejectedState.resizeRightTo(visibleStartTick + 100, visibleStartTick,
                                       lastValidProperties, timeline),
           "audio resize state must accept a valid edge before testing rejection");
    auto rejectedProperties = lastValidProperties;
    expect(!rejectedState.resizeRightTo(visibleStartTick, visibleStartTick, rejectedProperties,
                                        timeline) &&
               rejectedProperties.start == lastValidProperties.start &&
               rejectedProperties.clipStart == lastValidProperties.clipStart &&
               rejectedProperties.clipLen == lastValidProperties.clipLen,
           "a rejected audio resize must leave the last valid projected properties intact");

    if (g_failures == 0) {
        QTextStream(stdout) << "All TrackEditorInteractions tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
