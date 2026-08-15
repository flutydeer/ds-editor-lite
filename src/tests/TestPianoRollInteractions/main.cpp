#include "UI/Views/ClipEditor/PianoRoll/NoteEditUtils.h"
#include "UI/Views/Common/EditorSelectionUtils.h"

#include <lite/MusicBase/Timeline.h>

#include <QCoreApplication>
#include <QTextStream>

namespace {
    int g_failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    constexpr int startTick = 480;
    constexpr int quantize = 120;
    expect(NoteEditUtils::lengthForSnappedEnd(startTick, 960, quantize) == 480,
           "dragging right must extend a note to the current snapped endpoint");
    expect(NoteEditUtils::lengthForSnappedEnd(startTick, 720, quantize) == 240,
           "dragging back left must shorten a previously extended note");
    expect(NoteEditUtils::lengthForSnappedEnd(startTick, 480, quantize) == quantize &&
               NoteEditUtils::lengthForSnappedEnd(startTick, 240, quantize) == quantize,
           "drawing at or before the start must retain one quantization step");

    const Timeline timeline;
    constexpr int clipStart = 100;
    expect(NoteEditUtils::snapLocalDown(310.0, clipStart, quantize, timeline) == 140 &&
               NoteEditUtils::snapLocalNearest(310.0, clipStart, quantize, timeline) == 260,
           "draw/left-resize and right-resize must retain their legacy absolute snap policies");
    expect(NoteEditUtils::moveDelta(179.9, quantize) == 120 &&
               NoteEditUtils::moveDelta(181.0, quantize) == 240,
           "note movement must snap the pointer delta instead of the note's absolute start");
    expect(NoteEditUtils::leftResizeDelta(480, 240, 700, quantize) == 120 &&
               NoteEditUtils::rightResizeDelta(480, 240, 500, quantize) == -120,
           "note resizing must retain at least one quantization step");

    using EditorSelectionUtils::OrderedSelectionModel;
    constexpr auto ctrlShift = Qt::ControlModifier | Qt::ShiftModifier;
    const QList<int> orderedNotes{10, 20, 30, 40, 50};
    OrderedSelectionModel selection;
    auto result = selection.press({}, orderedNotes, 20, Qt::NoModifier);
    expect(result.selection == QList<int>{20} && result.targetSelected &&
               result.collapseToTargetOnRelease && selection.anchorId() == 20,
           "plain note press must establish the range anchor and select the target");

    result = selection.press(result.selection, orderedNotes, 40, Qt::ShiftModifier);
    expect(result.selection == (QList<int>{20, 30, 40}) && !result.collapseToTargetOnRelease &&
               selection.anchorId() == 20,
           "Shift press must replace selection with the ordered anchor range");

    result = selection.press({20, 50}, orderedNotes, 40, ctrlShift);
    expect(result.selection == (QList<int>{20, 30, 40, 50}) && selection.anchorId() == 20,
           "Ctrl+Shift press must add the anchor range to the existing selection");

    result = selection.press(result.selection, orderedNotes, 30, Qt::ControlModifier);
    expect(result.selection == (QList<int>{20, 40, 50}) && !result.targetSelected &&
               selection.anchorId() == 30,
           "Ctrl press must toggle the target and update the anchor");

    selection.synchronize(result.selection);
    expect(selection.anchorId() == 50,
           "external selection synchronization must replace a deselected anchor");
    result = selection.press(result.selection, orderedNotes, 10, Qt::ShiftModifier);
    expect(result.selection == orderedNotes,
           "reverse Shift range must use the synchronized anchor and temporal order");

    selection.clearAnchor();
    result = selection.press({20, 30}, orderedNotes, 40, Qt::ShiftModifier);
    expect(result.selection == QList<int>{40} && selection.anchorId() == 40,
           "Shift press without a valid anchor must fall back to a plain single selection");

    expect(OrderedSelectionModel::finalizeClick({10, 20, 30}, 20, false, true) == QList<int>{20} &&
               OrderedSelectionModel::finalizeClick({10, 20, 30}, 20, true, true) ==
                   (QList<int>{10, 20, 30}) &&
               OrderedSelectionModel::finalizeClick({10, 20, 30}, 20, false, false) ==
                   (QList<int>{10, 20, 30}),
           "click release must collapse only an unmoved plain press");

    expect(EditorSelectionUtils::selectionForPress({1, 2}, 2, false) == (QList<int>{1, 2}),
           "context-pressing a selected note must preserve its multi-selection");
    expect(EditorSelectionUtils::selectionForPress({1, 2}, 3, false) == QList<int>{3},
           "context-pressing an unselected note must replace the previous selection");
    expect(EditorSelectionUtils::selectionForPress({1, 2}, -1, false).isEmpty(),
           "context-pressing the piano-roll background must clear note selection");

    if (g_failures == 0) {
        QTextStream(stdout) << "All PianoRollInteractions tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
