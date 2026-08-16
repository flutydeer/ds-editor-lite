#include "UI/Views/ClipEditor/PianoRoll/NoteEditUtils.h"
#include "UI/Views/Common/EditorSelectionUtils.h"

#include <lite/MusicBase/Timeline.h>

#include <QApplication>
#include <QMouseEvent>
#include <QTextStream>
#include <QWidget>
#include <QtTest/QTest>

namespace {
    int g_failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }

    class NoteSelectionEventProbe final : public QWidget {
    public:
        QList<int> selection;
        EditorSelectionUtils::OrderedSelectionModel model;

    protected:
        void mousePressEvent(QMouseEvent *event) override {
            const auto index =
                qBound(0, qFloor(event->position().x() / 20.0), m_ordered.size() - 1);
            const auto result =
                model.press(selection, m_ordered, m_ordered.at(index), event->modifiers());
            selection = result.selection;
        }

        void mouseReleaseEvent(QMouseEvent *event) override {
            Q_UNUSED(event)
            selection = model.release(selection, false);
        }

    private:
        const QList<int> m_ordered{10, 20, 30, 40, 50};
    };
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

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
    expect(NoteEditUtils::leftResizeDelta(480, 240, 2000, 0) == 239 &&
               NoteEditUtils::rightResizeDelta(480, 240, -1000, 0) == -239,
           "note resizing must retain one tick even when an invalid minimum is supplied");

    using EditorSelectionUtils::OrderedSelectionModel;
    constexpr auto ctrlShift = Qt::ControlModifier | Qt::ShiftModifier;
    const QList<int> orderedNotes{10, 20, 30, 40, 50};
    OrderedSelectionModel selection;
    auto result = selection.press({}, orderedNotes, 20, Qt::NoModifier);
    expect(result.selection == QList<int>{20} && result.targetSelected &&
               selection.anchorId() == 20,
           "plain note press must establish the range anchor and select the target");
    result.selection = selection.release(result.selection, false);

    result = selection.press(result.selection, orderedNotes, 40, Qt::ShiftModifier);
    expect(result.selection == (QList<int>{20, 30, 40}) && selection.anchorId() == 20,
           "Shift press must replace selection with the ordered anchor range");
    result.selection = selection.release(result.selection, false);

    result = selection.press({20, 50}, orderedNotes, 40, ctrlShift);
    expect(result.selection == (QList<int>{20, 30, 40, 50}) && selection.anchorId() == 20,
           "Ctrl+Shift press must add the anchor range to the existing selection");
    result.selection = selection.release(result.selection, false);

    result = selection.press(result.selection, orderedNotes, 30, Qt::ControlModifier);
    expect(result.selection == (QList<int>{20, 40, 50}) && !result.targetSelected &&
               selection.anchorId() == 30,
           "Ctrl press must toggle the target and update the anchor");
    result.selection = selection.release(result.selection, false);

    selection.synchronize(result.selection);
    expect(selection.anchorId() == 50,
           "external selection synchronization must replace a deselected anchor");
    result = selection.press(result.selection, orderedNotes, 10, Qt::ShiftModifier);
    expect(result.selection == orderedNotes,
           "reverse Shift range must use the synchronized anchor and temporal order");
    result.selection = selection.release(result.selection, false);

    selection.clearAnchor();
    result = selection.press({20, 30}, orderedNotes, 40, Qt::ShiftModifier);
    expect(result.selection == QList<int>{40} && selection.anchorId() == 40,
           "Shift press without a valid anchor must fall back to a plain single selection");
    result.selection = selection.release(result.selection, false);

    OrderedSelectionModel clickSelection;
    (void) clickSelection.press({10, 20, 30}, orderedNotes, 20, Qt::NoModifier);
    const auto collapsed = clickSelection.release({10, 20, 30}, false);
    (void) clickSelection.press({10, 20, 30}, orderedNotes, 20, Qt::NoModifier);
    const auto dragged = clickSelection.release({10, 20, 30}, true);
    (void) clickSelection.press({10, 20, 30}, orderedNotes, 20, Qt::ControlModifier);
    const auto modified = clickSelection.release({10, 20, 30}, false);
    (void) clickSelection.press({10, 20, 30}, orderedNotes, 20, Qt::AltModifier);
    const auto altClick = clickSelection.release({10, 20, 30}, false);
    expect(collapsed == QList<int>{20} && dragged == (QList<int>{10, 20, 30}) &&
               modified == (QList<int>{10, 20, 30}) && altClick == QList<int>{20},
           "click release must collapse only an unmoved plain press");

    NoteSelectionEventProbe eventProbe;
    eventProbe.resize(100, 20);
    eventProbe.show();
    app.processEvents();
    QTest::mouseClick(&eventProbe, Qt::LeftButton, Qt::NoModifier, QPoint(25, 10));
    QTest::mouseClick(&eventProbe, Qt::LeftButton, Qt::ControlModifier, QPoint(65, 10));
    expect(eventProbe.selection == (QList<int>{20, 40}),
           "a real Ctrl+mouse gesture must preserve both notes after release");
    QTest::mouseClick(&eventProbe, Qt::LeftButton, Qt::ControlModifier | Qt::ShiftModifier,
                      QPoint(5, 10));
    expect(eventProbe.selection == (QList<int>{10, 20, 30, 40}),
           "a real Ctrl+Shift+mouse gesture must add the anchor range after release");
    QTest::mouseClick(&eventProbe, Qt::LeftButton, Qt::ShiftModifier, QPoint(85, 10));
    expect(eventProbe.selection == (QList<int>{40, 50}),
           "a real Shift+mouse gesture must preserve the anchor range after release");
    QTest::mouseClick(&eventProbe, Qt::LeftButton, Qt::ControlModifier, QPoint(65, 10));
    expect(eventProbe.selection == QList<int>{50},
           "a real Ctrl+mouse gesture must keep a toggled-off note deselected after release");

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
