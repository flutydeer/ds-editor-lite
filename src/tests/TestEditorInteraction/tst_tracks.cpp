#include "tst_editor_interaction.h"

#include <QtTest/QTest>
#include "UI/Views/TrackEditor/AudioClipDragState.h"
#include <lite/ProjectModel/Utils/ClipResizeUtils.h>
#include <lite/ProjectModel/Utils/NotePasteUtils.h>
#include <lite/ProjectModel/Utils/SingingClipRangeUtils.h>
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

    bool closeTo(const double left, const double right) {
        return std::abs(left - right) < 0.0001;
    }

    struct AudioDragFixture {
        static constexpr double trimStartMs = 500.0;
        static constexpr double playLengthMs = 2500.0;
        static constexpr double materialLengthMs = 5000.0;
        static constexpr int visibleStartTick = 4700;
        static constexpr int grabTick = 5100;

        Timeline timeline{
            {{0, 120.0}, {4800, 60.0}, {9600, 150.0}}
        };
        AudioClip::TickCaches caches = AudioClip::deriveTickCaches(
            trimStartMs, playLengthMs, materialLengthMs, visibleStartTick, timeline);

        Clip::ClipCommonProperties properties() const {
            Clip::ClipCommonProperties result;
            result.start = caches.start;
            result.clipStart = caches.clipStart;
            result.clipLen = caches.clipLen;
            result.length = caches.length;
            return result;
        }

        AudioClipDragState begin() const {
            return AudioClipDragState::begin(trimStartMs, playLengthMs, materialLengthMs,
                                             visibleStartTick, grabTick, timeline);
        }

        int rightTick() const {
            return visibleStartTick + caches.clipLen;
        }
    };

}

void EditorInteractionTests::resizeHitTesting() {
    using EditorResizeUtils::HorizontalEdge;
    QVERIFY2((EditorResizeUtils::horizontalEdgeAt(4.0, 100.0, 6.0) == HorizontalEdge::Left &&
              EditorResizeUtils::horizontalEdgeAt(96.0, 100.0, 6.0) == HorizontalEdge::Right &&
              EditorResizeUtils::horizontalEdgeAt(50.0, 100.0, 6.0) == HorizontalEdge::None),
             "note and clip resize handles must share the same horizontal edge hit test");
    QVERIFY2((EditorResizeUtils::horizontalEdgeAt(5.0, 8.0, 6.0) == HorizontalEdge::Left),
             "overlapping resize handles must retain left-edge precedence");
}

void EditorInteractionTests::clipSelection() {
    QVERIFY2((EditorSelectionUtils::selectionForPress({1}, 2, false) == QList<int>{2}),
             "pressing an unselected clip must replace the previous selection");
    QVERIFY2((EditorSelectionUtils::selectionForPress({1, 2}, 2, false) == (QList<int>{1, 2})),
             "pressing a selected clip must preserve its multi-selection");
    QVERIFY2((EditorSelectionUtils::selectionForPress({1}, 2, true) == (QList<int>{1, 2}) &&
              EditorSelectionUtils::selectionForPress({1}, 1, true).isEmpty()),
             "toggle presses must add or remove the target clip");
}

void EditorInteractionTests::singingClipRightResize() {
    Clip::ClipCommonProperties singing;
    singing.length = 1920;
    singing.clipStart = 120;
    singing.clipLen = 960;
    QVERIFY2((ClipResizeUtils::updateRightEdge(singing, 3000, 120, true, 1800)),
             "a positive singing clip resize must be accepted");
    QVERIFY2((singing.clipLen == 3000 && singing.length == 3120),
             "expanding a singing clip must extend its editable content length");
    QVERIFY2((ClipResizeUtils::updateRightEdge(singing, 600, 120, true, 1800)),
             "shrinking a singing clip must be accepted");
    QVERIFY2((singing.clipLen == 600 && singing.length == 1800),
             "shrinking must retain enough content length for existing notes");
}

void EditorInteractionTests::pasteExtendsVisibleRange() {
    Clip::ClipCommonProperties pasteTarget;
    pasteTarget.start = 960;
    pasteTarget.clipStart = 240;
    pasteTarget.length = 1200;
    pasteTarget.clipLen = 960;
    const NotePasteUtils::SourceBounds copiedBounds{120, 600};
    const auto beforeVisibleRange = NotePasteUtils::plan(pasteTarget, 1100, 1080, copiedBounds);
    QVERIFY2((beforeVisibleRange.localAnchor == 240 && beforeVisibleRange.offset == 120 &&
              beforeVisibleRange.pastedEnd == 720),
             "pasting in trimmed content must anchor the first note at the visible left edge");
    const auto insideVisibleRange = NotePasteUtils::plan(pasteTarget, 1490, 1440, copiedBounds);
    QVERIFY2((insideVisibleRange.localAnchor == 480 && insideVisibleRange.offset == 360),
             "pasting inside the visible range must use the snapped playback position");
    const auto overrunsVisibleRange = NotePasteUtils::plan(pasteTarget, 2020, 2040, copiedBounds);
    const auto paddedPasteEnd =
        SingingClipRangeUtils::paddedContentEnd(overrunsVisibleRange.pastedEnd);
    QVERIFY2(
        (overrunsVisibleRange.pastedEnd == 1560 &&
         SingingClipRangeUtils::extendRightToFit(pasteTarget, overrunsVisibleRange.pastedEnd) &&
         pasteTarget.clipStart + pasteTarget.clipLen == paddedPasteEnd &&
         pasteTarget.length == paddedPasteEnd),
        "inserting beyond the visible right edge must extend both clip ranges with tail room");
    const auto extendedPasteTarget = pasteTarget;
    QVERIFY2(
        (!SingingClipRangeUtils::extendRightToFit(pasteTarget, overrunsVisibleRange.pastedEnd) &&
         pasteTarget.clipLen == extendedPasteTarget.clipLen &&
         pasteTarget.length == extendedPasteTarget.length),
        "an existing tail margin must not grow again for unchanged note geometry");
}

void EditorInteractionTests::trimmedClipRetainsTailRoom() {
    Clip::ClipCommonProperties manuallyTrimmed;
    manuallyTrimmed.start = 4000;
    manuallyTrimmed.clipStart = 200;
    manuallyTrimmed.clipLen = 600;
    manuallyTrimmed.length = 1200;
    constexpr int touchingContentEnd = 800;
    const auto paddedTouchingEnd = SingingClipRangeUtils::paddedContentEnd(touchingContentEnd);
    QVERIFY2((SingingClipRangeUtils::extendRightToFit(manuallyTrimmed, touchingContentEnd) &&
              manuallyTrimmed.clipStart + manuallyTrimmed.clipLen == paddedTouchingEnd &&
              manuallyTrimmed.length == 1200),
             "editing a note touching a manually trimmed edge must restore the tail margin");
    QVERIFY2((paddedTouchingEnd - touchingContentEnd == SingingClipRangeUtils::tailPaddingTicks),
             "adaptive clip room must use the fixed tick padding");
}

void EditorInteractionTests::clipResizeBounds() {
    Clip::ClipCommonProperties leftResize;
    leftResize.start = 100;
    leftResize.clipStart = 200;
    leftResize.clipLen = 600;
    QVERIFY2((ClipResizeUtils::updateLeftEdge(leftResize, 500) && leftResize.clipStart == 400 &&
              leftResize.clipLen == 400),
             "left resize must preserve the visible right edge");
    QVERIFY2((ClipResizeUtils::updateLeftEdge(leftResize, 50) && leftResize.clipStart == 0 &&
              leftResize.clipLen == 800),
             "left resize must stop at the content origin");

    constexpr std::array gridSteps{60, 120, 240};
    for (const auto gridStep : gridSteps) {
        Clip::ClipCommonProperties crossedLeft;
        crossedLeft.start = 100;
        crossedLeft.clipStart = 200;
        crossedLeft.clipLen = 600;
        QVERIFY2((ClipResizeUtils::updateLeftEdge(crossedLeft, 1200, gridStep) &&
                  crossedLeft.clipLen == gridStep),
                 "left resize crossing the right edge must use the supplied grid step");
    }

    Clip::ClipCommonProperties audio;
    audio.length = 2000;
    audio.clipStart = 500;
    audio.clipLen = 500;
    QVERIFY2((ClipResizeUtils::updateRightEdge(audio, 3000, 120, false, 2000)),
             "a positive audio clip resize must be accepted");
    QVERIFY2((audio.clipLen == 1500 && audio.length == 2000),
             "audio resize must stop at the material boundary");
    for (const auto gridStep : gridSteps) {
        Clip::ClipCommonProperties crossedRight;
        crossedRight.length = 2000;
        crossedRight.clipStart = 500;
        crossedRight.clipLen = 500;
        QVERIFY2((ClipResizeUtils::updateRightEdge(crossedRight, -500, gridStep, false, 2000) &&
                  crossedRight.clipLen == gridStep),
                 "right resize crossing the left edge must use the supplied grid step");
    }
}

void EditorInteractionTests::minimumLengthAndContentBounds() {
    Clip::ClipCommonProperties unsnapped;
    unsnapped.length = 1000;
    unsnapped.clipLen = 500;
    QVERIFY2(
        (ClipResizeUtils::updateRightEdge(unsnapped, -100, 1, true, 0) && unsnapped.clipLen == 1),
        "unsnapped right resize must retain a positive one-tick length");

    struct ContentSpan {
        int start;
        int length;
    };

    const std::array overlappingContent = {
        ContentSpan{0,   1200},
        ContentSpan{800, 100 }
    };
    const auto contentEnd = ClipResizeUtils::furthestContentEnd(
        overlappingContent.cbegin(), overlappingContent.cend(), AppGlobal::ticksPerWholeNote,
        [](const ContentSpan &span) { return span.start + span.length; });
    QVERIFY2((contentEnd == 1200),
             "overlapping singing notes must retain the furthest endpoint, not the latest start");
    const std::array<ContentSpan, 0> emptyContent;
    QVERIFY2((ClipResizeUtils::furthestContentEnd(emptyContent.cbegin(), emptyContent.cend(),
                                                  AppGlobal::ticksPerWholeNote,
                                                  [](const ContentSpan &span) {
                                                      return span.start + span.length;
                                                  }) == AppGlobal::ticksPerWholeNote),
             "an empty singing clip must retain the default editable content length");
}

void EditorInteractionTests::clipPreviewLayout() {
    const QRectF preview(10.0, 20.0, 200.0, 80.0);
    const auto layout = SingingClipPreview::computeLayout(preview, {60, 64, 67});
    QVERIFY2((layout.valid()), "a note range must produce a valid preview layout");
    QVERIFY2((layout.lowestKeyIndex == 60 && layout.highestKeyIndex == 67),
             "preview layout must preserve the note range");
    QVERIFY2((closeTo(layout.noteHeight, 8.0)),
             "preview notes must use the classic backend's maximum height");
    QVERIFY2((closeTo(layout.contentTop, 28.0)),
             "a compact note range must be vertically centered in the preview");
    QVERIFY2((closeTo(layout.keyIndexAt(layout.contentTop), 67.0)),
             "the preview inverse mapping must return the highest key at the content top");
    QVERIFY2((closeTo(layout.keyIndexAt(layout.contentTop + 7.0 * layout.noteHeight), 60.0)),
             "the preview inverse mapping must return the lowest key at the last row");

    const auto physicalLayout =
        SingingClipPreview::computeLayout(QRectF(20.0, 40.0, 400.0, 160.0), {60, 67}, 16.0);
    QVERIFY2((closeTo(physicalLayout.noteHeight, 16.0) && closeTo(physicalLayout.contentTop, 56.0)),
             "device-pixel scaling must preserve the logical preview layout");
    QVERIFY2((!SingingClipPreview::computeLayout(preview, {}).valid()),
             "an empty note range must not produce a preview layout");
}

void EditorInteractionTests::projectedNotePreview() {
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
    QVERIFY2((projectedNotes == expectedNotes),
             "track previews must share replacement, insertion, erasure, and temporal ordering");
    QVERIFY2((SingingClipPreview::keyIndices(projectedNotes) == (QList<int>{60, 55, 72})),
             "preview layout keys must come from the projected note geometry");
    QVERIFY2((SingingClipPreview::projectNotes(modelNotes, false, editedNotes, {3}) == modelNotes),
             "piano-roll edits must not leak into inactive clip previews");
}

void EditorInteractionTests::audioMovePreservesRealTimeWindow() {
    const AudioDragFixture fixture;
    auto properties = fixture.properties();
    auto state = fixture.begin();
    const auto visibleStart = state.visibleStartForCursor(10500, fixture.timeline);
    state.moveTo(visibleStart, properties, fixture.timeline);
    QCOMPARE(properties.start + properties.clipStart, visibleStart);
    state.writeTruth(properties);
    QVERIFY(closeTo(properties.trimStartMs, fixture.trimStartMs));
    QVERIFY(closeTo(properties.playLengthMs, fixture.playLengthMs));
    QVERIFY(closeTo(properties.materialLengthMs, fixture.materialLengthMs));
}

void EditorInteractionTests::audioLeftTrimPreservesMaterialOriginAndRightEdge() {
    const AudioDragFixture fixture;
    auto properties = fixture.properties();
    auto state = fixture.begin();
    constexpr int newLeftTick = 5200;
    QVERIFY(state.resizeLeftTo(newLeftTick, fixture.rightTick(), 1, properties, fixture.timeline));
    state.writeTruth(properties);
    const auto materialStart =
        fixture.timeline.tickToMs(fixture.visibleStartTick) - fixture.trimStartMs;
    const auto originalEnd =
        fixture.timeline.tickToMs(fixture.visibleStartTick) + fixture.playLengthMs;
    QVERIFY(
        closeTo(properties.trimStartMs, fixture.timeline.tickToMs(newLeftTick) - materialStart));
    QVERIFY(closeTo(properties.playLengthMs, originalEnd - fixture.timeline.tickToMs(newLeftTick)));
}

void EditorInteractionTests::audioRightTrimStopsAtMaterialBoundary() {
    const AudioDragFixture fixture;
    auto properties = fixture.properties();
    auto state = fixture.begin();
    const auto materialStart =
        fixture.timeline.tickToMs(fixture.visibleStartTick) - fixture.trimStartMs;
    const auto beyondMaterial =
        qRound(fixture.timeline.msToTick(materialStart + fixture.materialLengthMs + 1000.0));
    QVERIFY(state.resizeRightTo(beyondMaterial, fixture.visibleStartTick, 1, properties,
                                fixture.timeline));
    state.writeTruth(properties);
    QVERIFY(closeTo(properties.playLengthMs, fixture.materialLengthMs - fixture.trimStartMs));
}

void EditorInteractionTests::audioResizeAcrossOppositeEdge_data() {
    QTest::addColumn<bool>("leftEdge");
    QTest::addColumn<int>("gridStep");
    for (const auto gridStep : {60, 120, 240}) {
        const auto leftName = QByteArrayLiteral("left-") + QByteArray::number(gridStep);
        const auto rightName = QByteArrayLiteral("right-") + QByteArray::number(gridStep);
        QTest::newRow(leftName.constData()) << true << gridStep;
        QTest::newRow(rightName.constData()) << false << gridStep;
    }
}

void EditorInteractionTests::audioResizeAcrossOppositeEdge() {
    QFETCH(bool, leftEdge);
    QFETCH(int, gridStep);
    const AudioDragFixture fixture;
    auto properties = fixture.properties();
    auto state = fixture.begin();
    const bool accepted =
        leftEdge ? state.resizeLeftTo(fixture.rightTick() + 1000, fixture.rightTick(), gridStep,
                                      properties, fixture.timeline)
                 : state.resizeRightTo(fixture.visibleStartTick - 1000, fixture.visibleStartTick,
                                       gridStep, properties, fixture.timeline);
    QVERIFY(accepted);
    QCOMPARE(properties.clipLen, gridStep);
}
