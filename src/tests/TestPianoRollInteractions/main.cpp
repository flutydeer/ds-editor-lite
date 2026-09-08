#include "UI/Views/ClipEditor/PianoRoll/NoteEditUtils.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteLyricPresentation.h"
#include "UI/Views/Common/EditorSelectionUtils.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QFontMetricsF>
#include <QTextStream>
#include <QtTest/QTest>

class PianoRollInteractionsTests final : public QObject {
    Q_OBJECT

private slots:

    void canonicalNoteOrder() {
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

            QVERIFY2(
                (clip.notes().toList() ==
                 (QList<Note *>{first, splitContinuation, sameStartHighFirst, sameStartHighSecond,
                                sameStartLow, later})),
                "model notes must use start, descending pitch, and id as their canonical order");
        }
    }

    void drawAndResizeGeometry() {
        constexpr int startTick = 480;
        constexpr int quantize = 120;
        QVERIFY2((NoteEditUtils::lengthForSnappedEnd(startTick, 960, quantize) == 480),
                 "dragging right must extend a note to the current snapped endpoint");
        QVERIFY2((NoteEditUtils::lengthForSnappedEnd(startTick, 720, quantize) == 240),
                 "dragging back left must shorten a previously extended note");
        QVERIFY2((NoteEditUtils::lengthForSnappedEnd(startTick, 480, quantize) == quantize &&
                  NoteEditUtils::lengthForSnappedEnd(startTick, 240, quantize) == quantize),
                 "drawing at or before the start must retain one quantization step");

        const Timeline timeline;
        constexpr int clipStart = 100;
        QVERIFY2(
            (NoteEditUtils::snapLocalDown(310.0, clipStart, quantize, timeline) == 140 &&
             NoteEditUtils::snapLocalNearest(310.0, clipStart, quantize, timeline) == 260),
            "draw/left-resize and right-resize must retain their legacy absolute snap policies");
        QVERIFY2((NoteEditUtils::moveDelta(179.9, quantize) == 120 &&
                  NoteEditUtils::moveDelta(181.0, quantize) == 240),
                 "note movement must snap the pointer delta instead of the note's absolute start");
        QVERIFY2((NoteEditUtils::leftResizeDelta(480, 240, 700, quantize) == 120 &&
                  NoteEditUtils::rightResizeDelta(480, 240, 500, quantize) == -120),
                 "note resizing must retain at least one quantization step");
        QVERIFY2((NoteEditUtils::leftResizeDelta(480, 240, 2000, 0) == 239 &&
                  NoteEditUtils::rightResizeDelta(480, 240, -1000, 0) == -239),
                 "note resizing must retain one tick even when an invalid minimum is supplied");
        QVERIFY2((NoteEditUtils::leftResizeDelta(0, 240, -999, quantize) == 0 &&
                  NoteEditUtils::leftResizeDelta(240, 240, 120, quantize) == -120),
                 "resizing the left edge must clamp at clip start (tick 0)");
        QVERIFY2((NoteResizeUtils::clampLeftMoveDelta(-480, 240) == -240 &&
                  NoteResizeUtils::clampLeftMoveDelta(-480, 0) == 0 &&
                  NoteResizeUtils::clampLeftMoveDelta(150, 480) == 150 &&
                  NoteResizeUtils::clampLeftMoveDelta(0, 0) == 0),
                 "moving notes must never push a local start below zero");
        constexpr int alternateQuantize = 80;
        QVERIFY2((NoteResizeUtils::clampLeftDelta(240, 240, quantize) == 120 &&
                  NoteResizeUtils::clampRightDelta(240, -240, quantize) == -120 &&
                  NoteResizeUtils::clampLeftDelta(240, 240, alternateQuantize) == 160 &&
                  NoteResizeUtils::clampRightDelta(240, -240, alternateQuantize) == -160 &&
                  NoteResizeUtils::clampLeftDelta(240, 240, 1) == 239 &&
                  NoteResizeUtils::clampRightDelta(240, -240, 1) == -239),
                 "the model commit guard must retain the supplied dynamic minimum length");
    }

    void lyricVisibility() {
        QFont lyricFont;
        lyricFont.setPixelSize(13);
        const QFontMetricsF lyricMetrics(lyricFont);
        const QString longLyric = QStringLiteral("extraordinary");
        const auto fullLyricWidth = lyricMetrics.horizontalAdvance(longLyric);
        const auto noteTextHeight = lyricMetrics.height() + 4.0;
        const auto wideLayout = NoteLyricPresentation::layout(
            QRectF(0.0, 0.0, fullLyricWidth + 12.0, noteTextHeight), longLyric, lyricFont, 1.0);
        QVERIFY2((wideLayout.displayText == longLyric && !wideLayout.elided),
                 "a lyric that fits must remain unchanged");
        QVERIFY2((!NoteLyricPresentation::isElidedInRect(wideLayout, longLyric, lyricFont,
                                                         wideLayout.textRect)),
                 "a fully visible lyric must not be tooltip-eligible");
        const auto clippedTextRect =
            wideLayout.textRect.adjusted(fullLyricWidth * 0.5, 0.0, 0.0, 0.0);
        QVERIFY2((NoteLyricPresentation::isElidedInRect(wideLayout, longLyric, lyricFont,
                                                        clippedTextRect)),
                 "a lyric clipped by the viewport must remain tooltip-eligible");

        const QRectF narrowNoteRect(0.0, 0.0, fullLyricWidth * 0.55, noteTextHeight);
        const auto narrowLayout =
            NoteLyricPresentation::layout(narrowNoteRect, longLyric, lyricFont, 1.0);
        QVERIFY2((narrowLayout.isVisible() && narrowLayout.elided &&
                  narrowLayout.displayText != longLyric),
                 "a long lyric must be right-elided instead of disappearing");
        const auto ultraShortLayout = NoteLyricPresentation::layout(
            QRectF(0.0, 0.0, 7.0, noteTextHeight), longLyric, lyricFont, 1.0);
        QVERIFY2((!ultraShortLayout.isVisible() && ultraShortLayout.elided),
                 "a lyric with no drawable width must remain eligible for its tooltip");
        const auto subpixelWidthLayout = NoteLyricPresentation::layout(
            QRectF(0.0, 0.0, 7.2, noteTextHeight), QStringLiteral("啦"), lyricFont, 1.0);
        QVERIFY2((subpixelWidthLayout.textRect.width() > 0.0 &&
                  subpixelWidthLayout.textRect.width() < 1.0 && !subpixelWidthLayout.isVisible() &&
                  subpixelWidthLayout.elided),
                 "a subpixel lyric area must stay tooltip-eligible across editor backends");
        const auto negativeWidthLayout = NoteLyricPresentation::layout(
            QRectF(0.0, 0.0, 6.0, noteTextHeight), QStringLiteral("啦"), lyricFont, 1.0);
        QVERIFY2((negativeWidthLayout.textRect.width() < 0.0 && !negativeWidthLayout.isVisible() &&
                  negativeWidthLayout.elided),
                 "an ultra-short note must not draw a one-character lyric outside its bounds");
        const auto compactLayout =
            NoteLyricPresentation::layout(narrowNoteRect, longLyric, lyricFont,
                                          NoteLyricPresentation::compactScaleThreshold - 0.01);
        QVERIFY2((!compactLayout.isVisible() && !compactLayout.elided),
                 "compact note rendering must suppress lyrics and tooltip eligibility");
    }

    void orderedSelection() {
        using EditorSelectionUtils::OrderedSelectionModel;
        constexpr auto ctrlShift = Qt::ControlModifier | Qt::ShiftModifier;
        const QList<int> orderedNotes{10, 20, 30, 40, 50};
        OrderedSelectionModel selection;
        auto result = selection.press({}, orderedNotes, 20, Qt::NoModifier);
        QVERIFY2((result.selection == QList<int>{20} && result.targetSelected &&
                  selection.anchorId() == 20),
                 "plain note press must establish the range anchor and select the target");
        result.selection = selection.release(result.selection, false);

        result = selection.press(result.selection, orderedNotes, 40, Qt::ShiftModifier);
        QVERIFY2((result.selection == (QList<int>{20, 30, 40}) && selection.anchorId() == 20),
                 "Shift press must replace selection with the ordered anchor range");
        result.selection = selection.release(result.selection, false);

        result = selection.press({20, 50}, orderedNotes, 40, ctrlShift);
        QVERIFY2((result.selection == (QList<int>{20, 30, 40, 50}) && selection.anchorId() == 20),
                 "Ctrl+Shift press must add the anchor range to the existing selection");
        result.selection = selection.release(result.selection, false);

        result = selection.press(result.selection, orderedNotes, 30, Qt::ControlModifier);
        QVERIFY2((result.selection == (QList<int>{20, 40, 50}) && !result.targetSelected &&
                  selection.anchorId() == 30),
                 "Ctrl press must toggle the target and update the anchor");
        result.selection = selection.release(result.selection, false);

        selection.synchronize(result.selection);
        QVERIFY2((selection.anchorId() == 50),
                 "external selection synchronization must replace a deselected anchor");
        result = selection.press(result.selection, orderedNotes, 10, Qt::ShiftModifier);
        QVERIFY2((result.selection == orderedNotes),
                 "reverse Shift range must use the synchronized anchor and temporal order");
        result.selection = selection.release(result.selection, false);

        selection.clearAnchor();
        result = selection.press({20, 30}, orderedNotes, 40, Qt::ShiftModifier);
        QVERIFY2((result.selection == QList<int>{40} && selection.anchorId() == 40),
                 "Shift press without a valid anchor must fall back to a plain single selection");
        result.selection = selection.release(result.selection, false);
    }

    void clickAndDragSelection() {
        using EditorSelectionUtils::OrderedSelectionModel;
        const QList<int> orderedNotes{10, 20, 30, 40, 50};
        OrderedSelectionModel clickSelection;
        (void) clickSelection.press({10, 20, 30}, orderedNotes, 20, Qt::NoModifier);
        const auto collapsed = clickSelection.release({10, 20, 30}, false);
        (void) clickSelection.press({10, 20, 30}, orderedNotes, 20, Qt::NoModifier);
        const auto dragged = clickSelection.release({10, 20, 30}, true);
        (void) clickSelection.press({10, 20, 30}, orderedNotes, 20, Qt::ControlModifier);
        const auto modified = clickSelection.release({10, 20, 30}, false);
        (void) clickSelection.press({10, 20, 30}, orderedNotes, 20, Qt::AltModifier);
        const auto altClick = clickSelection.release({10, 20, 30}, false);
        QVERIFY2((collapsed == QList<int>{20} && dragged == (QList<int>{10, 20, 30}) &&
                  modified == (QList<int>{10, 20, 30}) && altClick == QList<int>{20}),
                 "click release must collapse only an unmoved plain press");
    }

    void contextMenuSelection() {
        QVERIFY2((EditorSelectionUtils::selectionForPress({1, 2}, 2, false) == (QList<int>{1, 2})),
                 "context-pressing a selected note must preserve its multi-selection");
        QVERIFY2((EditorSelectionUtils::selectionForPress({1, 2}, 3, false) == QList<int>{3}),
                 "context-pressing an unselected note must replace the previous selection");
        QVERIFY2((EditorSelectionUtils::selectionForPress({1, 2}, -1, false).isEmpty()),
                 "context-pressing the piano-roll background must clear note selection");
    }
};

QTEST_MAIN(PianoRollInteractionsTests)
#include "main.moc"
