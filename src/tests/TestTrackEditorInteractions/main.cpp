#include "UI/Views/TrackEditor/AudioClipDragState.h"
#include "UI/Views/TrackEditor/ClipResizeUtils.h"
#include "UI/Views/TrackEditor/SingingClipPreviewLayout.h"
#include "UI/Views/Common/EditorResizeUtils.h"
#include "UI/Views/Common/EditorSelectionUtils.h"
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

    using EditorResizeUtils::HorizontalEdge;
    expect(EditorResizeUtils::horizontalEdgeAt(4.0, 100.0, 6.0) == HorizontalEdge::Left &&
               EditorResizeUtils::horizontalEdgeAt(96.0, 100.0, 6.0) == HorizontalEdge::Right &&
               EditorResizeUtils::horizontalEdgeAt(50.0, 100.0, 6.0) == HorizontalEdge::None,
           "note and clip resize handles must share the same horizontal edge hit test");
    expect(EditorResizeUtils::horizontalEdgeAt(5.0, 8.0, 6.0) == HorizontalEdge::Left,
           "overlapping resize handles must retain left-edge precedence");

    expect(EditorSelectionUtils::selectionForPress({1}, 2, false) == QList<int>{2},
           "pressing an unselected clip must replace the previous selection");
    expect(EditorSelectionUtils::selectionForPress({1, 2}, 2, false) == (QList<int>{1, 2}),
           "pressing a selected clip must preserve its multi-selection");
    expect(EditorSelectionUtils::selectionForPress({1}, 2, true) == (QList<int>{1, 2}) &&
               EditorSelectionUtils::selectionForPress({1}, 1, true).isEmpty(),
           "toggle presses must add or remove the target clip");

    Clip::ClipCommonProperties singing;
    singing.length = 1920;
    singing.clipStart = 120;
    singing.clipLen = 960;
    expect(ClipResizeUtils::updateRightEdge(singing, 3000, 120, true, 1800),
           "a positive singing clip resize must be accepted");
    expect(singing.clipLen == 3000 && singing.length == 3120,
           "expanding a singing clip must extend its editable content length");
    expect(ClipResizeUtils::updateRightEdge(singing, 600, 120, true, 1800),
           "shrinking a singing clip must be accepted");
    expect(singing.clipLen == 600 && singing.length == 1800,
           "shrinking must retain enough content length for existing notes");

    Clip::ClipCommonProperties leftResize;
    leftResize.start = 100;
    leftResize.clipStart = 200;
    leftResize.clipLen = 600;
    expect(ClipResizeUtils::updateLeftEdge(leftResize, 500) && leftResize.clipStart == 400 &&
               leftResize.clipLen == 400,
           "left resize must preserve the visible right edge");
    expect(ClipResizeUtils::updateLeftEdge(leftResize, 50) && leftResize.clipStart == 0 &&
               leftResize.clipLen == 800,
           "left resize must stop at the content origin");

    constexpr std::array gridSteps{60, 120, 240};
    for (const auto gridStep : gridSteps) {
        Clip::ClipCommonProperties crossedLeft;
        crossedLeft.start = 100;
        crossedLeft.clipStart = 200;
        crossedLeft.clipLen = 600;
        expect(ClipResizeUtils::updateLeftEdge(crossedLeft, 1200, gridStep) &&
                   crossedLeft.clipLen == gridStep,
               "left resize crossing the right edge must use the supplied grid step");
    }

    Clip::ClipCommonProperties audio;
    audio.length = 2000;
    audio.clipStart = 500;
    audio.clipLen = 500;
    expect(ClipResizeUtils::updateRightEdge(audio, 3000, 120, false, 2000),
           "a positive audio clip resize must be accepted");
    expect(audio.clipLen == 1500 && audio.length == 2000,
           "audio resize must stop at the material boundary");
    for (const auto gridStep : gridSteps) {
        Clip::ClipCommonProperties crossedRight;
        crossedRight.length = 2000;
        crossedRight.clipStart = 500;
        crossedRight.clipLen = 500;
        expect(ClipResizeUtils::updateRightEdge(crossedRight, -500, gridStep, false, 2000) &&
                   crossedRight.clipLen == gridStep,
               "right resize crossing the left edge must use the supplied grid step");
    }

    Clip::ClipCommonProperties unsnapped;
    unsnapped.length = 1000;
    unsnapped.clipLen = 500;
    expect(ClipResizeUtils::updateRightEdge(unsnapped, -100, 1, true, 0) &&
               unsnapped.clipLen == 1,
           "unsnapped right resize must retain a positive one-tick length");

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

    const QVector<EditorPreview::Note> modelNotes{
        {1, 0,   240, 60},
        {2, 480, 240, 64},
        {3, 960, 240, 67},
    };
    const QVector<EditorPreview::Note> editedNotes{
        {2,  1200, 480, 72},
        {-1, 360,  240, 55},
    };
    const QVector<EditorPreview::Note> expectedNotes{
        {1,  0,    240, 60},
        {-1, 360,  240, 55},
        {2,  1200, 480, 72},
    };
    const auto projectedNotes = SingingClipPreview::projectNotes(modelNotes, editedNotes, {3});
    expect(projectedNotes == expectedNotes,
           "track previews must share replacement, insertion, erasure, and temporal ordering");
    expect(SingingClipPreview::keyIndices(projectedNotes) == (QList<int>{60, 55, 72}),
           "preview layout keys must come from the projected note geometry");
    expect(SingingClipPreview::projectNotes(modelNotes, false, editedNotes, {3}) == modelNotes,
           "piano-roll edits must not leak into inactive clip previews");

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
    expect(leftState.resizeLeftTo(newLeftTick, visibleStartTick + initialCaches.clipLen, 1,
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
    expect(rightState.resizeRightTo(beyondMaterialTick, visibleStartTick, 1, draggedAudio,
                                    timeline),
           "audio right trim must accept an edge beyond the material boundary");
    rightState.writeTruth(draggedAudio);
    expect(closeTo(draggedAudio.playLengthMs, materialLengthMs - trimStartMs),
           "audio right trim must stop at the material boundary in realtime");

    const auto originalRightTick = visibleStartTick + initialCaches.clipLen;
    for (const auto gridStep : gridSteps) {
        auto crossedState = AudioClipDragState::begin(trimStartMs, playLengthMs, materialLengthMs,
                                                      visibleStartTick, grabTick, timeline);
        Clip::ClipCommonProperties crossedProperties;
        crossedProperties.start = initialCaches.start;
        crossedProperties.clipStart = initialCaches.clipStart;
        crossedProperties.clipLen = initialCaches.clipLen;
        crossedProperties.length = initialCaches.length;
        expect(crossedState.resizeRightTo(visibleStartTick - 1000, visibleStartTick, gridStep,
                                          crossedProperties, timeline) &&
                   crossedProperties.clipLen == gridStep,
               "audio right resize crossing the opposite edge must use the supplied grid step");

        auto crossedLeftState = AudioClipDragState::begin(
            trimStartMs, playLengthMs, materialLengthMs, visibleStartTick, grabTick, timeline);
        crossedProperties.start = initialCaches.start;
        crossedProperties.clipStart = initialCaches.clipStart;
        crossedProperties.clipLen = initialCaches.clipLen;
        crossedProperties.length = initialCaches.length;
        expect(crossedLeftState.resizeLeftTo(originalRightTick + 1000, originalRightTick,
                                             gridStep, crossedProperties, timeline) &&
                   crossedProperties.clipLen == gridStep,
               "audio left resize crossing the opposite edge must use the supplied grid step");
    }

    if (g_failures == 0) {
        QTextStream(stdout) << "All TrackEditorInteractions tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
