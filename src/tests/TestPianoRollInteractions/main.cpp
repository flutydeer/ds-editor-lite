#include "UI/Views/ClipEditor/PianoRoll/NoteDrawUtils.h"
#include "UI/Views/Common/EditorSelectionUtils.h"

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
    expect(NoteDrawUtils::lengthForSnappedEnd(startTick, 960, quantize) == 480,
           "dragging right must extend a note to the current snapped endpoint");
    expect(NoteDrawUtils::lengthForSnappedEnd(startTick, 720, quantize) == 240,
           "dragging back left must shorten a previously extended note");
    expect(NoteDrawUtils::lengthForSnappedEnd(startTick, 480, quantize) == quantize &&
               NoteDrawUtils::lengthForSnappedEnd(startTick, 240, quantize) == quantize,
           "drawing at or before the start must retain one quantization step");

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
