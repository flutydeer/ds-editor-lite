#include "tst_editor_interaction.h"

#include "UI/Views/ClipEditor/PianoRoll/NoteHandleGeometry.h"

#include <QtTest/QTest>

void EditorInteractionTests::touchNoteResizeFramesExposeOnlyTheSideBands() {
    using EditorResizeUtils::HorizontalEdge;
    const QRectF modelRect(40, 10, 96, 24);
    const auto inner = NoteHandleGeometry::innerRect(modelRect);
    const auto outer = NoteHandleGeometry::outerRect(modelRect);
    const auto leftBandX = (outer.left() + inner.left()) * 0.5;
    const auto rightBandX = (outer.right() + inner.right()) * 0.5;
    const auto centerY = inner.center().y();
    QVERIFY(NoteHandleGeometry::sideBandContains(modelRect, QPointF(leftBandX, centerY)));
    QVERIFY(NoteHandleGeometry::sideBandContains(modelRect, QPointF(rightBandX, centerY)));
    QVERIFY(!NoteHandleGeometry::sideBandContains(modelRect,
                                                  QPointF(inner.center().x(), outer.top() + 0.5)));
    QVERIFY(!NoteHandleGeometry::sideBandContains(modelRect, QPointF(outer.left() - 1, centerY)));
    QCOMPARE(NoteHandleGeometry::resizeEdgeAt(QPointF(leftBandX, centerY), modelRect, 8, true),
             HorizontalEdge::Left);
    QCOMPARE(NoteHandleGeometry::resizeEdgeAt(QPointF(rightBandX, centerY), modelRect, 8, true),
             HorizontalEdge::Right);
    QCOMPARE(NoteHandleGeometry::resizeEdgeAt(
                 QPointF(modelRect.left() - NoteHandleGeometry::grabExpansion - 0.5, centerY),
                 modelRect, 8, true),
             HorizontalEdge::None);
    QCOMPARE(NoteHandleGeometry::resizeEdgeAt(modelRect.center(), modelRect, 8, true),
             HorizontalEdge::None);
    QCOMPARE(NoteHandleGeometry::resizeEdgeAt(QPointF(modelRect.left() + 4, 0), modelRect, 8, true),
             HorizontalEdge::Left);
    QCOMPARE(
        NoteHandleGeometry::resizeEdgeAt(QPointF(modelRect.right() - 4, 0), modelRect, 8, true),
        HorizontalEdge::Right);
    QVERIFY(NoteHandleGeometry::outerRect(QRectF()).isEmpty());
    QVERIFY(NoteHandleGeometry::gripRect(QRectF(), false).isEmpty());
    for (double x : {-9., -1., 0., 4., 8., 47., 88., 92., 96., 100.}) {
        QCOMPARE(
            NoteHandleGeometry::resizeEdgeAt(QPointF(modelRect.left() + x, 0), modelRect, 8, false),
            EditorResizeUtils::horizontalEdgeAt(x, modelRect.width(), 8));
    }
}

void EditorInteractionTests::touchNoteResizeFrameVisibilityRespectsTheEditingContext() {
    using ClipEditorGlobal::DrawNote;
    using ClipEditorGlobal::EraseNote;
    using ClipEditorGlobal::Select;
    QVERIFY(NoteHandleGeometry::toolAllowsHandles(Select));
    QVERIFY(NoteHandleGeometry::toolAllowsHandles(ClipEditorGlobal::IntervalSelect));
    QVERIFY(NoteHandleGeometry::toolAllowsHandles(DrawNote));
    QVERIFY(!NoteHandleGeometry::toolAllowsHandles(EraseNote));
    QVERIFY(!NoteHandleGeometry::toolAllowsHandles(ClipEditorGlobal::SplitNote));
    QVERIFY(!NoteHandleGeometry::toolAllowsHandles(ClipEditorGlobal::DrawPitch));
    QVERIFY(!NoteHandleGeometry::toolAllowsHandles(ClipEditorGlobal::EditPitchAnchor));
    QVERIFY(NoteHandleGeometry::frameVisible(true, 1, Select, false));
    QVERIFY(!NoteHandleGeometry::frameVisible(false, 1, Select, false));
    QVERIFY(!NoteHandleGeometry::frameVisible(true, 2, Select, false));
    QVERIFY(!NoteHandleGeometry::frameVisible(true, 0, Select, false));
    QVERIFY(!NoteHandleGeometry::frameVisible(true, 1, EraseNote, false));
    QVERIFY(!NoteHandleGeometry::frameVisible(true, 1, Select, true));
}
