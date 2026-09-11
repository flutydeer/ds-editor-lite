#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/ClipController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/CommonParamEditorView.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsScene.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/ParamProperties.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>
#include <QtTest/QTest>

#include <algorithm>

namespace {
    SingingClip *defaultSingingClip(AppModel &model) {
        for (const auto *track : model.tracks()) {
            for (auto *clip : track->clips()) {
                if (auto *singing = dynamic_cast<SingingClip *>(clip))
                    return singing;
            }
        }
        return nullptr;
    }

    int valueAt(const QList<DrawCurve *> &curves, int tick) {
        for (const auto *curve : curves) {
            if (curve->localStart() <= tick && tick < curve->localEndTick())
                return curve->values().at((tick - curve->localStart()) / curve->step);
        }
        return -1;
    }

    struct ParameterEditorFixture {
        explicit ParameterEditorFixture(SingingClip *clip)
            : view(&scene, foregroundProperties, backgroundProperties) {
            QObject::connect(&view, &TimeGraphicsView::sizeChanged, &scene,
                             &ParamEditorGraphicsScene::onViewResized);
            view.setForeground(ParamInfo::MouthOpening, foregroundProperties);
            view.setDataContext(clip);
            view.setEditMode(ParamEditorEditMode::Draw);
            view.setAnimationEnabled(false);
            view.resize(900, 300);
            view.show();
            view.activateWindow();
            view.setFocus();
            for (auto *item : scene.items()) {
                auto *editor = dynamic_cast<CommonParamEditorView *>(item);
                if (editor && !editor->transparentMouseEvents()) {
                    foreground = editor;
                    break;
                }
            }
        }

        ~ParameterEditorFixture() {
            view.setDataContext(nullptr);
        }

        QPoint pointFor(int tick, double value) const {
            const auto visible = view.visibleRect();
            const auto ratio = (tick - view.startTick()) / (view.endTick() - view.startTick());
            return view.mapFromScene(QPointF(visible.left() + ratio * visible.width(),
                                             foreground->sceneYForValue(value)));
        }

        void moveWithLeftButton(const QPoint &position) {
            QMouseEvent event(QEvent::MouseMove, QPointF(position),
                              QPointF(view.viewport()->mapToGlobal(position)), Qt::NoButton,
                              Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view.viewport(), &event);
        }

        MouthOpeningParamProperties foregroundProperties;
        TensionParamProperties backgroundProperties;
        ParamEditorGraphicsScene scene;
        ParamEditorGraphicsView view;
        CommonParamEditorView *foreground = nullptr;
    };
}

void ApplicationGuiTests::parameterAnchorEditingPreviewsAndUsesTheContextMenu() {
    auto *clip = defaultSingingClip(*context->m_appModel);
    QVERIFY(clip);
    clipController->setClip(clip);
    appStatus->activeClipId = clip->id();
    ParameterEditorFixture editor(clip);
    QVERIFY(editor.foreground);
    QTRY_VERIFY(editor.view.isActiveWindow() && editor.scene.height() > 200);
    QVERIFY(editor.view.setViewportScale(2.0, 1.0));
    editor.view.setViewportStartTick(0);
    editor.view.setEditMode(ParamEditorEditMode::Anchor);
    QCoreApplication::processEvents();
    auto *parameter = clip->params.getParamByName(ParamInfo::MouthOpening);
    QVERIFY(parameter && parameter->curves(Param::Edited).isEmpty());
    const auto curve = [&] {
        return dynamic_cast<const AnchorCurve *>(parameter->curves(Param::Edited).value(0));
    };
    auto &runtime = *context->m_coreRuntime;
    historyManager->reset();
    const auto before = runtime.documentVersion();
    auto *viewport = editor.view.viewport();
    const auto first = editor.pointFor(480, 200);
    const auto last = editor.pointFor(960, 800);
    QVERIFY(viewport->rect().contains(first) && viewport->rect().contains(last));
    const auto begin = [&] {
        QTest::mouseMove(viewport, first);
        QTest::mouseDClick(viewport, Qt::LeftButton, Qt::NoModifier, first);
        QTest::mouseRelease(viewport, Qt::LeftButton, Qt::NoModifier, first);
    };
    begin();
    QVERIFY(editSessionManager->hasActiveTransaction());
    QVERIFY(!curve());
    const auto firstPreview = viewport->grab().toImage();
    QTest::mouseMove(viewport, last);
    const auto segmentPreview = viewport->grab().toImage();
    QVERIFY(!segmentPreview.isNull() && segmentPreview != firstPreview);
    QCOMPARE(runtime.documentVersion(), before);
    QTest::keyClick(&editor.view, Qt::Key_Escape);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(!curve());
    QVERIFY(!historyManager->canUndo());
    begin();
    QTest::mouseMove(viewport, last);
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, last);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(curve());
    const auto nodes = curve()->nodes().toList();
    QCOMPARE(nodes.size(), 2);
    QVERIFY(qAbs(nodes.first()->pos() - 480) <= 4);
    QVERIFY(qAbs(nodes.last()->pos() - 960) <= 4);
    QVERIFY(qAbs(nodes.first()->value() - 200) <= 5);
    QVERIFY(qAbs(nodes.last()->value() - 800) <= 5);
    const auto originalInterpolation = nodes.first()->interpMode();
    const auto selectedInterpolation =
        originalInterpolation == AnchorNode::Linear ? AnchorNode::Hermite : AnchorNode::Linear;
    const auto label = ParamEditorGraphicsView::tr(
        selectedInterpolation == AnchorNode::Linear ? "Linear" : "Hermite");
    bool menuUsed = false;
    QTimer choose;
    choose.setInterval(10);
    connect(&choose, &QTimer::timeout, &editor.view, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (!menu)
            return;
        choose.stop();
        const auto close = qScopeGuard([&] { menu->close(); });
        QAction *choice = nullptr;
        for (auto *action : menu->actions()) {
            if (action->text() == label)
                choice = action;
        }
        QVERIFY(choice && choice->isEnabled());
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                          menu->actionGeometry(choice).center());
        menuUsed = true;
    });
    QTest::mouseMove(viewport, first);
    QContextMenuEvent contextMenu(QContextMenuEvent::Mouse, first, viewport->mapToGlobal(first));
    choose.start();
    QApplication::sendEvent(viewport, &contextMenu);
    QVERIFY(menuUsed);
    QVERIFY(curve());
    QCOMPARE(curve()->nodes().toList().first()->interpMode(), selectedInterpolation);
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(curve());
    QCOMPARE(curve()->nodes().toList().first()->interpMode(), originalInterpolation);
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(!curve());
    QVERIFY(!historyManager->canUndo());
    QVERIFY(clip->params.getParamByName(ParamInfo::Tension)->curves(Param::Edited).isEmpty());
}

void ApplicationGuiTests::parameterStrokeCommitsOnceAndUndoRestoresView() {
    auto *clip = defaultSingingClip(*context->m_appModel);
    QVERIFY(clip);
    clipController->setClip(clip);
    appStatus->activeClipId = clip->id();
    ParameterEditorFixture editor(clip);
    QVERIFY(editor.foreground);
    QTRY_VERIFY(editor.view.isVisible() && editor.scene.height() > 200);
    QVERIFY(editor.view.setViewportScale(1.0, 1.0));
    editor.view.setViewportStartTick(0);
    QCoreApplication::processEvents();
    auto *parameter = clip->params.getParamByName(ParamInfo::MouthOpening);
    QVERIFY(parameter);
    QVERIFY(parameter->curves(Param::Edited).isEmpty());
    QVERIFY(editor.foreground->editedCurves().isEmpty());
    historyManager->reset();
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto press = editor.pointFor(480, 500);
    const auto release = editor.pointFor(960, 500);
    QVERIFY(editor.view.viewport()->rect().contains(press));
    QVERIFY(editor.view.viewport()->rect().contains(release));
    QSignalSpy started(editor.foreground, &CommonParamEditorView::editStarted);
    QSignalSpy committed(editor.foreground, &CommonParamEditorView::editCommitted);

    QTest::mousePress(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, press);
    editor.moveWithLeftButton(release);
    QTRY_VERIFY(!editor.foreground->editedCurves().isEmpty());
    QCOMPARE(started.count(), 1);
    QCOMPARE(committed.count(), 0);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(appStatus->currentEditObject.get(), AppStatus::EditObjectType::Param);
    QVERIFY(parameter->curves(Param::Edited).isEmpty());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QTRY_COMPARE(parameter->curves(Param::Edited).size(), 1);
    QCOMPARE(committed.count(), 1);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(appStatus->currentEditObject.get(), AppStatus::EditObjectType::None);
    const auto *curve = dynamic_cast<const DrawCurve *>(parameter->curves(Param::Edited).first());
    QVERIFY(curve);
    QCOMPARE(curve->localStart(), 480);
    QCOMPARE(curve->localEndTick(), 960);
    QVERIFY(std::all_of(curve->values().cbegin(), curve->values().cend(),
                        [](int value) { return value == 500; }));
    const auto committedValues = curve->values();
    const auto committedStep = curve->step;
    QCOMPARE(editor.foreground->editedCurves().size(), 1);
    QCOMPARE(editor.foreground->editedCurves().first()->localStart(), curve->localStart());
    QCOMPARE(editor.foreground->editedCurves().first()->values(), committedValues);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(historyManager->canUndo());
    QVERIFY(!historyManager->isOnSavePoint());

    QVERIFY(runtime.history().undo(commandContext()));
    QTRY_VERIFY(parameter->curves(Param::Edited).isEmpty());
    QTRY_VERIFY(editor.foreground->editedCurves().isEmpty());
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
    QVERIFY(historyManager->isOnSavePoint());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);

    QVERIFY(runtime.history().redo(commandContext()));
    QTRY_COMPARE(parameter->curves(Param::Edited).size(), 1);
    const auto *restored =
        dynamic_cast<const DrawCurve *>(parameter->curves(Param::Edited).first());
    QVERIFY(restored);
    QCOMPARE(restored->localStart(), 480);
    QCOMPARE(restored->step, committedStep);
    QCOMPARE(restored->values(), committedValues);
    QCOMPARE(editor.foreground->editedCurves().first()->values(), committedValues);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 3);
}

void ApplicationGuiTests::escapeCancelsParameterStrokeWithoutChangingDocument() {
    auto *clip = defaultSingingClip(*context->m_appModel);
    QVERIFY(clip);
    clipController->setClip(clip);
    appStatus->activeClipId = clip->id();
    ParameterEditorFixture editor(clip);
    QVERIFY(editor.foreground);
    QTRY_VERIFY(editor.view.isVisible() && editor.scene.height() > 200);
    QVERIFY(editor.view.setViewportScale(1.0, 1.0));
    editor.view.setViewportStartTick(0);
    QCoreApplication::processEvents();
    auto *parameter = clip->params.getParamByName(ParamInfo::MouthOpening);
    QVERIFY(parameter);
    const auto press = editor.pointFor(480, 500);
    const auto release = editor.pointFor(960, 500);
    QVERIFY(editor.view.viewport()->rect().contains(press));
    QVERIFY(editor.view.viewport()->rect().contains(release));
    QTest::mousePress(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, press);
    editor.moveWithLeftButton(release);
    QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QTRY_COMPARE(parameter->curves(Param::Edited).size(), 1);
    const auto *baseline =
        dynamic_cast<const DrawCurve *>(parameter->curves(Param::Edited).first());
    QVERIFY(baseline);
    const auto baselineValues = baseline->values();
    const auto baselineStart = baseline->localStart();
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto *historyEntry = historyManager->nextUndoEntry();
    QVERIFY(historyEntry);
    QSignalSpy committed(editor.foreground, &CommonParamEditorView::editCommitted);
    QSignalSpy discarded(editor.foreground, &CommonParamEditorView::editDiscarded);

    const auto overwriteStart = editor.pointFor(480, 750);
    const auto overwriteEnd = editor.pointFor(960, 750);
    QTest::mousePress(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, overwriteStart);
    editor.moveWithLeftButton(overwriteEnd);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QVERIFY(editor.foreground->editedCurves().first()->values() != baselineValues);
    QCOMPARE(baseline->values(), baselineValues);
    QCOMPARE(runtime.documentVersion(), before);

    QTest::keyClick(&editor.view, Qt::Key_Escape);
    QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, overwriteEnd);
    QCOMPARE(discarded.count(), 1);
    QCOMPARE(committed.count(), 0);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(appStatus->currentEditObject.get(), AppStatus::EditObjectType::None);
    QCOMPARE(editor.foreground->editedCurves().size(), 1);
    QCOMPARE(editor.foreground->editedCurves().first()->localStart(), baselineStart);
    QCOMPARE(editor.foreground->editedCurves().first()->values(), baselineValues);
    QCOMPARE(parameter->curves(Param::Edited).size(), 1);
    QCOMPARE(parameter->curves(Param::Edited).first(), baseline);
    QCOMPARE(baseline->values(), baselineValues);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), historyEntry);
    QVERIFY(historyManager->canUndo());
    QVERIFY(!historyManager->canRedo());
}

void ApplicationGuiTests::parameterTransformGesturesCommitAndCancel_data() {
    QTest::addColumn<ParamEditorEditMode>("mode");
    QTest::newRow("shape") << ParamEditorEditMode::Shape;
    QTest::newRow("scale") << ParamEditorEditMode::Scale;
}

void ApplicationGuiTests::parameterTransformGesturesCommitAndCancel() {
    QFETCH(ParamEditorEditMode, mode);
    auto *clip = defaultSingingClip(*context->m_appModel);
    QVERIFY(clip);
    clipController->setClip(clip);
    appStatus->activeClipId = clip->id();
    auto &runtime = *context->m_coreRuntime;
    Automation::CurveDraftDto draft;
    draft.type = Automation::CurveDraftDto::Type::Draw;
    draft.localStart = 240;
    draft.step = 5;
    for (int tick = 240; tick < 1200; tick += draft.step)
        draft.values.append(tick >= 600 && tick < 840 ? 600 : 300);
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), Automation::ClipId(clip->id()),
                                                  ParamInfo::MouthOpening, Param::Edited, {draft}));
    auto *parameter = clip->params.getParamByName(ParamInfo::MouthOpening);
    QVERIFY(parameter);
    ParameterEditorFixture editor(clip);
    QVERIFY(editor.foreground);
    QTRY_VERIFY(editor.view.isVisible() && editor.scene.height() > 200);
    QVERIFY(editor.view.setViewportScale(1.0, 1.0));
    editor.view.setViewportStartTick(0);
    editor.view.setEditMode(mode);
    QCoreApplication::processEvents();
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto snapshot = [&] {
        QList<DrawCurve> curves;
        for (const auto *curve : parameter->curves(Param::Edited))
            curves.append(*static_cast<const DrawCurve *>(curve));
        return curves;
    };
    const auto baseline = snapshot();
    QCOMPARE(baseline.size(), 1);
    const auto selectRange = [&] {
        const auto start = editor.pointFor(480, 500);
        const auto end = editor.pointFor(960, 500);
        QVERIFY(editor.view.viewport()->rect().contains(start));
        QVERIFY(editor.view.viewport()->rect().contains(end));
        QTest::mousePress(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, start);
        editor.moveWithLeftButton(end);
        QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, end);
        QVERIFY(!editSessionManager->hasActiveTransaction());
    };
    selectRange();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QSignalSpy committed(editor.foreground, &CommonParamEditorView::editCommitted);
    QSignalSpy discarded(editor.foreground, &CommonParamEditorView::editDiscarded);
    const auto press = editor.pointFor(720, 500);
    const auto release = press + QPoint(0, 50);
    QVERIFY(editor.view.viewport()->rect().contains(release));
    const auto cancelOnFailure = qScopeGuard([&] {
        if (editSessionManager->hasActiveTransaction()) {
            QTest::keyClick(&editor.view, Qt::Key_Escape);
            QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, release);
        }
    });
    QTest::mousePress(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, press);
    editor.moveWithLeftButton(release);
    QVERIFY(editSessionManager->hasActiveTransaction());
    const auto previewCenter = valueAt(editor.foreground->editedCurves(), 720);
    QVERIFY(previewCenter > 0 && previewCenter < 600);
    QCOMPARE(valueAt(editor.foreground->editedCurves(), 240), 300);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(snapshot(), baseline);
    QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(committed.count(), 1);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    const auto changed = snapshot();
    QVERIFY(changed != baseline);
    QCOMPARE(valueAt(editor.foreground->editedCurves(), 720), previewCenter);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(snapshot(), baseline);
    QCOMPARE(valueAt(editor.foreground->editedCurves(), 720), 600);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(snapshot(), changed);
    QCOMPARE(valueAt(editor.foreground->editedCurves(), 720), previewCenter);

    const auto beforeCancel = runtime.documentVersion();
    const auto *historyEntry = historyManager->nextUndoEntry();
    selectRange();
    if (QTest::currentTestFailed())
        return;
    QTest::mousePress(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, press);
    editor.moveWithLeftButton(release);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QVERIFY(valueAt(editor.foreground->editedCurves(), 720) < previewCenter);
    QTest::keyClick(&editor.view, Qt::Key_Escape);
    QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(discarded.count(), 1);
    QCOMPARE(committed.count(), 1);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(snapshot(), changed);
    QCOMPARE(valueAt(editor.foreground->editedCurves(), 720), previewCenter);
    QCOMPARE(runtime.documentVersion(), beforeCancel);
    QCOMPARE(historyManager->nextUndoEntry(), historyEntry);
}

void ApplicationGuiTests::parameterTransformHandlesControlTheTransitionRange() {
    auto *clip = defaultSingingClip(*context->m_appModel);
    QVERIFY(clip);
    clipController->setClip(clip);
    appStatus->activeClipId = clip->id();
    auto &runtime = *context->m_coreRuntime;
    Automation::CurveDraftDto draft;
    draft.localStart = 240;
    draft.step = 5;
    draft.values = QList<int>(192, 600);
    QVERIFY(runtime.parameters().replaceParameter(commandContext(), Automation::ClipId(clip->id()),
                                                  ParamInfo::MouthOpening, Param::Edited, {draft}));
    ParameterEditorFixture editor(clip);
    QVERIFY(editor.foreground);
    QTRY_VERIFY(editor.view.isActiveWindow() && editor.scene.height() > 200);
    QVERIFY(editor.view.setViewportScale(1.0, 1.0));
    editor.view.setViewportStartTick(0);
    editor.view.setEditMode(ParamEditorEditMode::Scale);
    QCoreApplication::processEvents();
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto baseline = context->m_appModel->serialize();
    QSignalSpy committed(editor.foreground, &CommonParamEditorView::editCommitted);
    const auto dragRange = [&](int fromTick, int toTick) {
        const auto start = editor.pointFor(fromTick, 500);
        const auto end = editor.pointFor(toTick, 500);
        QVERIFY(editor.view.viewport()->rect().contains(start));
        QVERIFY(editor.view.viewport()->rect().contains(end));
        QTest::mousePress(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, start);
        editor.moveWithLeftButton(end);
        QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, end);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(context->m_appModel->serialize(), baseline);
        QVERIFY(committed.isEmpty());
    };
    dragRange(480, 960);
    if (QTest::currentTestFailed())
        return;
    const auto selectionImage = editor.view.viewport()->grab().toImage();
    // The default 60 ms shoulders span 55 ticks at 120 BPM.
    for (const auto [from, to] :
         {qMakePair(425, 360), qMakePair(480, 600), qMakePair(1015, 1080), qMakePair(960, 840)}) {
        dragRange(from, to);
        if (QTest::currentTestFailed())
            return;
    }
    QVERIFY(editor.view.viewport()->grab().toImage() != selectionImage);
    const auto top = editor.foreground->sceneBoundingRect().top();
    const QPoint handle(editor.pointFor(720, 500).x(),
                        editor.view.mapFromScene(QPointF(0, top + 20)).y());
    const auto end = handle + QPoint(0, 50);
    QVERIFY(editor.view.viewport()->rect().contains(handle));
    QVERIFY(editor.view.viewport()->rect().contains(end));
    const auto cancel = qScopeGuard([&] {
        if (editSessionManager->hasActiveTransaction()) {
            QTest::keyClick(&editor.view, Qt::Key_Escape);
            QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, end);
        }
    });
    QTest::mousePress(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, handle);
    editor.moveWithLeftButton(end);
    QVERIFY(editSessionManager->hasActiveTransaction());
    const auto &preview = editor.foreground->editedCurves();
    const auto coreValue = valueAt(preview, 720);
    QVERIFY(coreValue > 0 && coreValue < 600);
    QCOMPARE(valueAt(preview, 300), 600);
    // Moving a core boundary carries its existing shoulder width.
    QCOMPARE(valueAt(preview, 420), 600);
    QVERIFY(valueAt(preview, 540) > coreValue && valueAt(preview, 540) < 600);
    QVERIFY(valueAt(preview, 900) > coreValue && valueAt(preview, 900) < 600);
    QCOMPARE(valueAt(preview, 1020), 600);
    QCOMPARE(valueAt(preview, 1140), 600);
    QCOMPARE(context->m_appModel->serialize(), baseline);
    QTest::mouseRelease(editor.view.viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QCOMPARE(committed.size(), 1);
    QCOMPARE(valueAt(editor.foreground->editedCurves(), 720), coreValue);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(context->m_appModel->serialize(), baseline);
    QCOMPARE(valueAt(editor.foreground->editedCurves(), 720), 600);
    QVERIFY(!historyManager->canUndo());
}
