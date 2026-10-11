#include "tst_editor_interaction.h"

#include "Model/AppStatus/AppStatus.h"

#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

void EditorInteractionTests::documentScopedStateReset() {
    auto *status = AppStatus::instance();
    const auto loopBefore = status->loopSettings.get();
    const auto trackCollapsedBefore = status->trackPanelCollapsed.get();
    const auto bottomCollapsedBefore = status->bottomPanelCollapsed.get();
    const auto trackAvailableBefore = status->trackAutoPageTurnAvailable.get();
    const auto pianoAvailableBefore = status->pianoRollAutoPageTurnAvailable.get();
    const auto restore = qScopeGuard([&] {
        status->resetDocumentScopedState();
        status->loopSettings = loopBefore;
        status->trackPanelCollapsed = trackCollapsedBefore;
        status->bottomPanelCollapsed = bottomCollapsedBefore;
        status->trackAutoPageTurnAvailable = trackAvailableBefore;
        status->pianoRollAutoPageTurnAvailable = pianoAvailableBefore;
    });
    status->selectedTrackIndex = 2;
    status->activeClipId = 41;
    status->selectedClips = QList<int>{41, 42};
    status->primarySelectedClipId = 41;
    status->selectedNotes = QList<int>{7, 8};
    status->primarySelectedNoteId = 7;
    status->currentEditObject = AppStatus::EditObjectType::Param;
    status->pianoRollQuantize = 8;
    status->pianoRollQuantizeEnabled = false;
    status->trackAutoPageTurnEnabled = false;
    status->pianoRollAutoPageTurnEnabled = false;
    status->pianoRollVisibleRect = QRectF(1, 2, 3, 4);
    status->pianoRollNoteEditPreview = QVector<AppStatus::NoteEditPreview>{{9, 100, 200, 60}};
    status->pianoRollNoteErasePreview = QList<int>{9};
    status->projectEditableLength = 12345;
    const LoopSettings loop(true, 480, 960);
    status->loopSettings = loop;
    status->trackPanelCollapsed = true;
    status->bottomPanelCollapsed = true;
    status->trackAutoPageTurnAvailable = false;
    status->pianoRollAutoPageTurnAvailable = false;

    QSignalSpy trackChanged(status, &AppStatus::selectedTrackIndexChanged);
    QSignalSpy clipChanged(status, &AppStatus::activeClipIdChanged);
    QSignalSpy clipsChanged(status, &AppStatus::clipSelectionChanged);
    QSignalSpy notesChanged(status, &AppStatus::noteSelectionChanged);
    QSignalSpy quantizeChanged(status, &AppStatus::pianoRollQuantizeChanged);
    QSignalSpy loopChanged(status, &AppStatus::loopSettingsChanged);
    status->resetDocumentScopedState();
    QCOMPARE(status->selectedTrackIndex.get(), -1);
    QCOMPARE(status->activeClipId.get(), -1);
    QVERIFY(status->selectedClips.get().isEmpty() && status->selectedNotes.get().isEmpty());
    QCOMPARE(status->primarySelectedClipId.get(), -1);
    QCOMPARE(status->primarySelectedNoteId.get(), -1);
    QCOMPARE(status->currentEditObject.get(), AppStatus::EditObjectType::None);
    QVERIFY(status->pianoRollNoteEditPreview.get().isEmpty());
    QVERIFY(status->pianoRollNoteErasePreview.get().isEmpty());
    QCOMPARE(status->pianoRollVisibleRect.get(), QRectF());
    QCOMPARE(status->projectEditableLength.get(), AppGlobal::ticksPerWholeNote * 100);
    QCOMPARE(status->pianoRollQuantize.get(), 16);
    QVERIFY(status->pianoRollQuantizeEnabled.get());
    QVERIFY(status->trackAutoPageTurnEnabled.get() && status->pianoRollAutoPageTurnEnabled.get());
    QCOMPARE(status->loopSettings.get(), loop);
    QVERIFY(status->trackPanelCollapsed.get() && status->bottomPanelCollapsed.get());
    QVERIFY(!status->trackAutoPageTurnAvailable.get() &&
            !status->pianoRollAutoPageTurnAvailable.get());
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(clipChanged.count(), 1);
    QCOMPARE(clipsChanged.count(), 1);
    QCOMPARE(notesChanged.count(), 1);
    QCOMPARE(quantizeChanged.count(), 1);
    QCOMPARE(loopChanged.count(), 0);
    status->resetDocumentScopedState();
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(clipChanged.count(), 1);
    QCOMPARE(clipsChanged.count(), 1);
    QCOMPARE(notesChanged.count(), 1);
    QCOMPARE(quantizeChanged.count(), 1);
}
