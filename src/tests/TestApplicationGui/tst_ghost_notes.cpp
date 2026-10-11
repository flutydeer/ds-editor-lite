#include "tst_application_gui.h"
#include "../TestSupport/OptionsPanelFixture.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "UI/Dialogs/Options/Pages/AppearancePage.h"
#include "UI/Views/ClipEditor/PianoRoll/GhostNoteOverlay.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"

#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QLabel>
#include <QScopeGuard>
#include <QtTest/QTest>

void ApplicationGuiTests::ghostReferenceSwitchUpdatesTheCanvasAndKeepsInputOnTheHost() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    const auto originalAppearance = settings.get().appearance;
    const auto restore =
        qScopeGuard([&] { QVERIFY(runtime.settings().updateAppearance({}, originalAppearance)); });
    auto appearance = originalAppearance;
    appearance.showGhostNotes = true;
    QVERIFY(runtime.settings().updateAppearance({}, appearance));
    Automation::TrackDraftDto referenceTrack;
    referenceTrack.name = QStringLiteral("Reference only");
    referenceTrack.colorIndex = 3;
    referenceTrack.resolveColorIndex = false;
    Automation::ClipDraftDto referenceClip;
    referenceClip.properties.length = 3840;
    referenceClip.properties.clipLen = 3840;
    referenceClip.defaultLanguage = QStringLiteral("eng");
    Automation::NoteDraftDto note;
    note.localStart = 1920;
    note.length = 240;
    note.keyIndex = 60;
    note.lyric = QStringLiteral("la");
    note.language = QStringLiteral("eng");
    referenceClip.notes.append(note);
    referenceTrack.clips.append(referenceClip);
    const auto inserted = runtime.project().insertTrack(commandContext(), 1, referenceTrack);
    QVERIFY(inserted);
    QCOMPARE(inserted.get().affectedObjects.size(), 1);
    const auto referenceTrackId = Automation::TrackId(inserted.get().affectedObjects.first().value);
    auto *track = context->m_appModel->findTrackById(referenceTrackId.value());
    QVERIFY(track);
    QCOMPARE(track->clips().count(), 1);
    auto *reference = qobject_cast<SingingClip *>(*track->clips().begin());
    QVERIFY(reference);
    QCOMPARE(reference->notes().count(), 1);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    GhostNoteOverlay *overlay = nullptr;
    for (auto *item : scene->items()) {
        if (auto *ghost = dynamic_cast<GhostNoteOverlay *>(item))
            overlay = ghost;
    }
    QVERIFY(overlay);
    QTRY_VERIFY(overlay->isVisible());
    // Reference collection is coalesced onto the next event-loop turn.
    QCoreApplication::processEvents();
    const auto position = pointFor(2040, 60);
    QVERIFY(view->viewport()->rect().contains(position));
    const QRect sample(position - QPoint(3, 1), QSize(7, 3));
    const auto enabledImage = view->viewport()->grab(sample).toImage();
    QVERIFY(!enabledImage.isNull());
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    historyManager->reset();
    const auto referenceNoteId = (*reference->notes().begin())->id();

    const auto toggle = [&](bool enabled) {
        AppOptionsDialog panel;
        TestSupport::openOptionsPage(panel, AppOptionsGlobal::Appearance);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<AppearancePage *>();
        QVERIFY(page);
        SwitchButton *control = nullptr;
        for (auto *label : page->findChildren<QLabel *>()) {
            if (label->text() == AppearancePage::tr("Show notes from other tracks"))
                control = qobject_cast<SwitchButton *>(label->buddy());
        }
        QVERIFY(control);
        QCOMPARE(control->value(), !enabled);
        page->ensureWidgetVisible(control);
        QTest::mouseClick(control, Qt::LeftButton);
        QTRY_COMPARE(control->value(), enabled);
        const auto updated = runtime.settings().getSettings();
        QVERIFY(updated);
        QCOMPARE(updated.get().appearance.showGhostNotes, enabled);
        panel.close();
        view->activateWindow();
        QTRY_VERIFY(view->isActiveWindow());
    };
    toggle(false);
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(!overlay->isVisible());
    QTRY_VERIFY(view->viewport()->grab(sample).toImage() != enabledImage);
    toggle(true);
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(overlay->isVisible());
    QTRY_COMPARE(view->viewport()->grab(sample).toImage(), enabledImage);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());

    view->setFocus();
    QTRY_VERIFY(view->hasFocus());
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QCOMPARE(singingClip->notes().count(), 1);
    QCOMPARE(reference->notes().count(), 1);
    QCOMPARE((*reference->notes().begin())->id(), referenceNoteId);
    QCOMPARE((*reference->notes().begin())->localStart(), 1920);
    const auto *drawn = *singingClip->notes().begin();
    QCOMPARE(drawn->keyIndex(), 60);
    QVERIFY(drawn->localStart() >= 1920 && drawn->localStart() < 2160);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
}
