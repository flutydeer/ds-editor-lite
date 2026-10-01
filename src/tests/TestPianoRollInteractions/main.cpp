#include "UI/Views/ClipEditor/PianoRoll/NoteEditUtils.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteHandleGeometry.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteLyricPresentation.h"
#include "UI/Views/Common/EditorSelectionUtils.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QFontMetricsF>
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

    {
        SingingClip clip;
        auto makeNote = [](const int start, const int key) {
            auto *note = new Note;
            note->setLocalStart(start);
            note->setLength(120);
            note->setKeyIndex(key);
            return note;
        };
        auto *first = makeNote(0, 60);
        auto *sameStartHighFirst = makeNote(240, 72);
        auto *sameStartHighSecond = makeNote(240, 72);
        auto *sameStartLow = makeNote(240, 60);
        auto *later = makeNote(480, 60);
        auto *splitContinuation = makeNote(120, 60);
        clip.insertNotes({later, sameStartHighSecond, sameStartLow, first, splitContinuation,
                          sameStartHighFirst});

        expect(clip.notes().toList() ==
                   (QList<Note *>{first, splitContinuation, sameStartHighFirst,
                                  sameStartHighSecond, sameStartLow, later}),
               "model notes must use start, descending pitch, and id as their canonical order");
    }

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
    expect(NoteEditUtils::leftResizeDelta(0, 240, -999, quantize) == 0 &&
               NoteEditUtils::leftResizeDelta(240, 240, 120, quantize) == -120,
           "resizing the left edge must clamp at clip start (tick 0)");
    expect(NoteResizeUtils::clampLeftMoveDelta(-480, 240) == -240 &&
               NoteResizeUtils::clampLeftMoveDelta(-480, 0) == 0 &&
               NoteResizeUtils::clampLeftMoveDelta(150, 480) == 150 &&
               NoteResizeUtils::clampLeftMoveDelta(0, 0) == 0,
           "moving notes must never push a local start below zero");
    constexpr int alternateQuantize = 80;
    expect(NoteResizeUtils::clampLeftDelta(240, 240, quantize) == 120 &&
               NoteResizeUtils::clampRightDelta(240, -240, quantize) == -120 &&
               NoteResizeUtils::clampLeftDelta(240, 240, alternateQuantize) == 160 &&
               NoteResizeUtils::clampRightDelta(240, -240, alternateQuantize) == -160 &&
               NoteResizeUtils::clampLeftDelta(240, 240, 1) == 239 &&
               NoteResizeUtils::clampRightDelta(240, -240, 1) == -239,
           "the model commit guard must retain the supplied dynamic minimum length");

    QFont lyricFont;
    lyricFont.setPixelSize(13);
    const QFontMetricsF lyricMetrics(lyricFont);
    const QString longLyric = QStringLiteral("extraordinary");
    const auto fullLyricWidth = lyricMetrics.horizontalAdvance(longLyric);
    const auto noteTextHeight = lyricMetrics.height() + 4.0;
    const auto wideLayout = NoteLyricPresentation::layout(
        QRectF(0.0, 0.0, fullLyricWidth + 12.0, noteTextHeight), longLyric, lyricFont, 1.0);
    expect(wideLayout.displayText == longLyric && !wideLayout.elided,
           "a lyric that fits must remain unchanged");
    expect(!NoteLyricPresentation::isElidedInRect(wideLayout, longLyric, lyricFont,
                                                   wideLayout.textRect),
           "a fully visible lyric must not be tooltip-eligible");
    const auto clippedTextRect =
        wideLayout.textRect.adjusted(fullLyricWidth * 0.5, 0.0, 0.0, 0.0);
    expect(NoteLyricPresentation::isElidedInRect(wideLayout, longLyric, lyricFont,
                                                  clippedTextRect),
           "a lyric clipped by the viewport must remain tooltip-eligible");

    const QRectF narrowNoteRect(0.0, 0.0, fullLyricWidth * 0.55, noteTextHeight);
    const auto narrowLayout =
        NoteLyricPresentation::layout(narrowNoteRect, longLyric, lyricFont, 1.0);
    expect(narrowLayout.isVisible() && narrowLayout.elided && narrowLayout.displayText != longLyric,
           "a long lyric must be right-elided instead of disappearing");
    const auto ultraShortLayout = NoteLyricPresentation::layout(
        QRectF(0.0, 0.0, 7.0, noteTextHeight), longLyric, lyricFont, 1.0);
    expect(!ultraShortLayout.isVisible() && ultraShortLayout.elided,
           "a lyric with no drawable width must remain eligible for its tooltip");
    const auto subpixelWidthLayout = NoteLyricPresentation::layout(
        QRectF(0.0, 0.0, 7.2, noteTextHeight), QStringLiteral("啦"), lyricFont, 1.0);
    expect(subpixelWidthLayout.textRect.width() > 0.0 &&
               subpixelWidthLayout.textRect.width() < 1.0 && !subpixelWidthLayout.isVisible() &&
               subpixelWidthLayout.elided,
           "a subpixel lyric area must stay tooltip-eligible across editor backends");
    const auto negativeWidthLayout = NoteLyricPresentation::layout(
        QRectF(0.0, 0.0, 6.0, noteTextHeight), QStringLiteral("啦"), lyricFont, 1.0);
    expect(negativeWidthLayout.textRect.width() < 0.0 && !negativeWidthLayout.isVisible() &&
               negativeWidthLayout.elided,
           "an ultra-short note must not draw a one-character lyric outside its bounds");
    const auto compactLayout = NoteLyricPresentation::layout(
        narrowNoteRect, longLyric, lyricFont, NoteLyricPresentation::compactScaleThreshold - 0.01);
    expect(!compactLayout.isVisible() && !compactLayout.elided,
           "compact note rendering must suppress lyrics and tooltip eligibility");

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

    // --- Touch resize handle ring ---------------------------------------------
    {
        using EditorResizeUtils::HorizontalEdge;
        constexpr auto sideBand = NoteHandleGeometry::sideBandWidth;
        constexpr auto capBand = NoteHandleGeometry::capBandWidth;
        const QRectF modelRect(40.0, 10.0, 96.0, 24.0);
        const auto inner = NoteHandleGeometry::innerRect(modelRect);
        const auto outer = NoteHandleGeometry::outerRect(modelRect);
        const auto visual = NoteHandleGeometry::visualRect(modelRect);
        expect(inner == visual && !outer.isEmpty(),
               "the ring's hole must be exactly the note's visible rect");
        expect(qFuzzyCompare(outer.width(), inner.width() + sideBand * 2.0) &&
                   qFuzzyCompare(outer.height(), inner.height() + capBand * 2.0),
               "the ring's side bands must be wider than its cap bands");
        expect(qFuzzyCompare(outer.center().x(), inner.center().x()) &&
                   qFuzzyCompare(outer.center().y(), inner.center().y()),
               "the ring must stay centered on the note");
        expect(qFuzzyCompare(NoteHandleGeometry::innerRadius(modelRect),
                             EditorItemGeometry::adaptiveCornerRadius(
                                 inner, EditorItemGeometry::noteCornerRadius)) &&
                   qFuzzyCompare(NoteHandleGeometry::outerRadius(modelRect),
                                 NoteHandleGeometry::innerRadius(modelRect) + capBand),
               "the hole's corners must follow the note's own radius and the outer corners must "
               "grow by the thin cap band");

        // Hit testing only recognizes the left/right vertical bands
        const auto leftBandX = (outer.left() + inner.left()) * 0.5;
        const auto rightBandX = (outer.right() + inner.right()) * 0.5;
        const auto centerY = inner.center().y();
        expect(NoteHandleGeometry::sideBandContains(modelRect, QPointF(leftBandX, centerY)) &&
                   NoteHandleGeometry::sideBandContains(modelRect, QPointF(rightBandX, centerY)),
               "both side bands of the ring must be grab targets");
        expect(!NoteHandleGeometry::sideBandContains(modelRect, QPointF(inner.center().x(),
                                                                       outer.top() + 0.5)),
               "the ring's cap bands must stay decoration (a press there would resolve to the "
               "row above and jump the note)");
        expect(!NoteHandleGeometry::sideBandContains(
                   modelRect, QPointF(outer.left() - 1.0, centerY)),
               "the grab zone must stop at the ring's outer edge");

        // The grab zone must line up with the drawn vertical bands
        const auto expansion = NoteHandleGeometry::grabExpansion;
        expect(NoteHandleGeometry::resizeEdgeAt(QPointF(leftBandX, centerY), modelRect, 8.0,
                                                true) == HorizontalEdge::Left &&
                   NoteHandleGeometry::resizeEdgeAt(QPointF(rightBandX, centerY), modelRect, 8.0,
                                                    true) == HorizontalEdge::Right,
               "the drawn bands must be the grab targets");
        expect(NoteHandleGeometry::resizeEdgeAt(
                   QPointF(modelRect.left() - expansion - 0.5, centerY), modelRect, 8.0,
                   true) == HorizontalEdge::None &&
                   NoteHandleGeometry::resizeEdgeAt(modelRect.center(), modelRect, 8.0, true) ==
                       HorizontalEdge::None,
               "the grab zone must stop at the outer edge of the ring");
        expect(NoteHandleGeometry::resizeEdgeAt(QPointF(modelRect.left() + 4.0, 0.0), modelRect, 8.0,
                                                true) == HorizontalEdge::Left &&
                   NoteHandleGeometry::resizeEdgeAt(QPointF(modelRect.right() - 4.0, 0.0),
                                                    modelRect, 8.0, true) == HorizontalEdge::Right,
               "the tolerance zone inside the note must keep resizing the matching edge");
        expect(NoteHandleGeometry::outerRect(QRectF(0.0, 0.0, 0.0, 0.0)).isEmpty(),
               "a degenerate note must not draw a ring");

        // The grip indicator line centered in each vertical band
        const auto leftGrip = NoteHandleGeometry::gripRect(modelRect, false);
        const auto rightGrip = NoteHandleGeometry::gripRect(modelRect, true);
        const auto leftBandCenter = (outer.left() + inner.left()) * 0.5;
        const auto rightBandCenter = (inner.right() + outer.right()) * 0.5;
        expect(qFuzzyCompare(leftGrip.center().x(), leftBandCenter) &&
                   qFuzzyCompare(rightGrip.center().x(), rightBandCenter),
               "each grip line must be centered in its own side band");
        expect(qFuzzyCompare(leftGrip.height(), outer.height() / 3.0) &&
                   qFuzzyCompare(rightGrip.height(), outer.height() / 3.0) &&
                   qFuzzyCompare(leftGrip.center().y(), outer.center().y()) &&
                   qFuzzyCompare(rightGrip.center().y(), outer.center().y()),
               "each grip line must be one third of the ring's height and vertically centered");
        expect(qFuzzyCompare(leftGrip.width(), NoteHandleGeometry::gripWidth) &&
                   leftGrip.left() > outer.left() && leftGrip.right() < inner.left() &&
                   rightGrip.left() > inner.right() && rightGrip.right() < outer.right(),
               "each grip line must be a hairline that stays inside its side band");
        expect(NoteHandleGeometry::gripRect(QRectF(0.0, 0.0, 0.0, 0.0), false).isEmpty(),
               "a degenerate note must not draw a grip line");

        // Mouse and pen (framesActive false) must stay point-for-point identical to before
        for (const auto x : {-9.0, -1.0, 0.0, 4.0, 8.0, 47.0, 88.0, 92.0, 96.0, 100.0}) {
            const auto legacy =
                EditorResizeUtils::horizontalEdgeAt(x, modelRect.width(), 8.0);
            expect(NoteHandleGeometry::resizeEdgeAt(QPointF(modelRect.left() + x, 0.0), modelRect,
                                                    8.0, false) == legacy,
                   "without handle frames the resize hit test must stay unchanged");
        }

        // Visibility contract
        using ClipEditorGlobal::DrawNote;
        using ClipEditorGlobal::EraseNote;
        using ClipEditorGlobal::Select;
        expect(NoteHandleGeometry::toolAllowsHandles(Select) &&
                   NoteHandleGeometry::toolAllowsHandles(ClipEditorGlobal::IntervalSelect) &&
                   NoteHandleGeometry::toolAllowsHandles(DrawNote) &&
                   !NoteHandleGeometry::toolAllowsHandles(EraseNote) &&
                   !NoteHandleGeometry::toolAllowsHandles(ClipEditorGlobal::SplitNote) &&
                   !NoteHandleGeometry::toolAllowsHandles(ClipEditorGlobal::DrawPitch) &&
                   !NoteHandleGeometry::toolAllowsHandles(ClipEditorGlobal::EditPitchAnchor),
               "only the note tools may show the resize handles");
        expect(NoteHandleGeometry::frameVisible(true, 1, Select, false),
               "a single selected note under a note tool must show the frame");
        expect(!NoteHandleGeometry::frameVisible(false, 1, Select, false),
               "a precise pointer must not see the frame (it has a hover cursor)");
        expect(!NoteHandleGeometry::frameVisible(true, 2, Select, false),
               "a multi-selection must not show a frame");
        expect(!NoteHandleGeometry::frameVisible(true, 0, Select, false),
               "an empty selection must not show a frame");
        expect(!NoteHandleGeometry::frameVisible(true, 1, EraseNote, false),
               "a non-note tool must not show the frame");
        expect(!NoteHandleGeometry::frameVisible(true, 1, Select, true),
               "the inline lyric editor must hide the frame it would sit on");
    }

    if (g_failures == 0) {
        QTextStream(stdout) << "All PianoRollInteractions tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
