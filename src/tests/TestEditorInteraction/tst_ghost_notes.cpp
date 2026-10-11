#include "tst_editor_interaction.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/ProjectSnapshot.h"

#include "Automation/CoreRuntime.h"
#include "UI/Views/ClipEditor/PianoRoll/GhostNoteLayer.h"
#include "UI/Views/ClipEditor/PianoRoll/GhostNoteSource.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCoreApplication>
#include <QEvent>
#include <QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>
#include <cmath>
#include <optional>

namespace {
    using namespace Automation;

    ClipDraftDto singingDraft(int start, int noteStart, int length, int key) {
        ClipDraftDto clip;
        clip.properties.start = start;
        clip.properties.length = 3840;
        clip.properties.clipLen = 3840;
        clip.defaultLanguage = QStringLiteral("eng");
        NoteDraftDto note;
        note.localStart = noteStart;
        note.length = length;
        note.keyIndex = key;
        note.lyric = QStringLiteral("la");
        note.language = QStringLiteral("eng");
        clip.notes.append(note);
        return clip;
    }

    struct GhostScene {
        ~GhostScene() {
            if (originalAppearance)
                QVERIFY(runtime().settings().updateAppearance({.source = InvocationSource::Test},
                                                              *originalAppearance));
        }

        CoreRuntime &runtime() const {
            return *document.context->m_coreRuntime;
        }

        CommandContext command() const {
            return {.expected = runtime().documentVersion(), .source = InvocationSource::Test};
        }

        void initialize() {
            QVERIFY2(document.initialize(), qPrintable(document.error));
            const auto settings = runtime().settings().getSettings();
            QVERIFY(settings);
            originalAppearance = settings.get().appearance;
            auto appearance = *originalAppearance;
            appearance.showGhostNotes = true;
            QVERIFY(runtime().settings().updateAppearance({.source = InvocationSource::Test},
                                                          appearance));
            QVERIFY(runtime().documents().commitNewDocument(
                command(), DocumentAutomationFacade::newDocumentDraft(false)));
            TrackDraftDto hostTrack;
            hostTrack.name = QStringLiteral("Host");
            hostTrack.colorIndex = 1;
            hostTrack.resolveColorIndex = false;
            hostTrack.clips = {singingDraft(960, 120, 240, 60), singingDraft(2400, 0, 120, 72)};
            TrackDraftDto first;
            first.name = QStringLiteral("Long reference");
            first.colorIndex = 4;
            first.resolveColorIndex = false;
            first.clips = {singingDraft(480, 0, 960, 60)};
            TrackDraftDto second;
            second.name = QStringLiteral("Later reference");
            second.colorIndex = 8;
            second.resolveColorIndex = false;
            second.clips = {singingDraft(1920, 240, 240, 64)};
            QVERIFY(runtime().project().insertTracks(command(), 0, {hostTrack, first, second}));
            const auto &tracks = document.context->m_appModel->tracks();
            host = qobject_cast<SingingClip *>(*tracks.at(0)->clips().begin());
            referenceA = qobject_cast<SingingClip *>(*tracks.at(1)->clips().begin());
            referenceB = qobject_cast<SingingClip *>(*tracks.at(2)->clips().begin());
            QVERIFY(host && referenceA && referenceB);
            trackA = TrackId(tracks.at(1)->id());
            trackB = TrackId(tracks.at(2)->id());
            noteA = NoteId((*referenceA->notes().begin())->id());
            noteB = NoteId((*referenceB->notes().begin())->id());
            QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
        }

        void setEnabled(bool enabled) {
            auto appearance = *originalAppearance;
            appearance.showGhostNotes = enabled;
            QVERIFY(runtime().settings().updateAppearance({.source = InvocationSource::Test},
                                                          appearance));
        }

        GuiDocumentFixture document;
        std::optional<AppearanceSettingsDto> originalAppearance;
        QPointer<SingingClip> host;
        QPointer<SingingClip> referenceA;
        QPointer<SingingClip> referenceB;
        TrackId trackA;
        TrackId trackB;
        NoteId noteA;
        NoteId noteB;
    };

    void flushGhostChanges(GhostNoteSource &source) {
        QCoreApplication::sendPostedEvents(&source, QEvent::MetaCall);
    }

    QRectF referenceBounds(const QVector<EditorRhiSolidVertex> &vertices, const QColor &color) {
        const auto alpha = static_cast<float>(color.alphaF());
        bool found = false;
        QRectF bounds;
        for (const auto &vertex : vertices) {
            if (std::abs(vertex.a - alpha) > 1e-5 ||
                std::abs(vertex.r - color.redF() * alpha) > 1e-5 ||
                std::abs(vertex.g - color.greenF() * alpha) > 1e-5 ||
                std::abs(vertex.b - color.blueF() * alpha) > 1e-5)
                continue;
            const QPointF point(vertex.x, vertex.y);
            if (!found) {
                bounds = QRectF(point, point);
                found = true;
            } else {
                bounds.setCoords(
                    std::min(bounds.left(), point.x()), std::min(bounds.top(), point.y()),
                    std::max(bounds.right(), point.x()), std::max(bounds.bottom(), point.y()));
            }
        }
        return bounds;
    }
}

void EditorInteractionTests::ghostNotesFollowDocumentChangesAndOptions() {
    GhostScene scene;
    scene.initialize();
    if (QTest::currentTestFailed())
        return;
    GhostNoteSource source;
    QSignalSpy changes(&source, &GhostNoteSource::changed);
    source.setHostClip(scene.host);
    flushGhostChanges(source);
    QVERIFY(source.enabled());
    const QList<GhostNote> initial{
        {480,  960, 60, 4},
        {2160, 240, 64, 8}
    };
    QCOMPARE(source.notes(), initial);
    QCOMPARE(source.maxLength(), 960);
    changes.clear();
    const auto hostNote = NoteId((*scene.host->notes().begin())->id());
    QVERIFY(scene.runtime().notes().moveNotes(scene.command(), ClipId(scene.host->id()), {hostNote},
                                              120, 0));
    QVERIFY(scene.runtime().notes().setLyric(scene.command(), ClipId(scene.referenceA->id()),
                                             scene.noteA, QStringLiteral("changed word")));
    flushGhostChanges(source);
    QCOMPARE(source.notes(), initial);
    QCOMPARE(changes.size(), 0);

    QVERIFY(scene.runtime().notes().moveNotes(scene.command(), ClipId(scene.referenceB->id()),
                                              {scene.noteB}, -120, 1));
    flushGhostChanges(source);
    QCOMPARE(source.notes(), (QList<GhostNote>{
                                 {480,  960, 60, 4},
                                 {2040, 240, 65, 8}
    }));
    QCOMPARE(changes.size(), 1);
    QVERIFY(scene.runtime().project().setTrackColor(scene.command(), scene.trackB, 6));
    flushGhostChanges(source);
    QCOMPARE(source.notes().last().colorIndex, 6);

    const auto beforeOptions = TestSupport::projectSnapshot(*scene.document.context->m_appModel);
    const auto versionBeforeOptions = scene.runtime().documentVersion();
    scene.setEnabled(false);
    flushGhostChanges(source);
    QVERIFY(!source.enabled());
    QVERIFY(source.notes().isEmpty());
    QCOMPARE(source.maxLength(), 0);
    QCOMPARE(TestSupport::projectSnapshot(*scene.document.context->m_appModel), beforeOptions);
    QCOMPARE(scene.runtime().documentVersion(), versionBeforeOptions);
    QVERIFY(scene.runtime().notes().moveNotes(scene.command(), ClipId(scene.referenceB->id()),
                                              {scene.noteB}, 240, 0));
    flushGhostChanges(source);
    QVERIFY(source.notes().isEmpty());
    scene.setEnabled(true);
    flushGhostChanges(source);
    QCOMPARE(source.notes(), (QList<GhostNote>{
                                 {480,  960, 60, 4},
                                 {2280, 240, 65, 6}
    }));

    const auto referenceAId = ClipId(scene.referenceA->id());
    QVERIFY(scene.runtime().project().removeTracks(scene.command(), {scene.trackA}));
    flushGhostChanges(source);
    QVERIFY(!scene.document.context->m_appModel->findClipById(referenceAId.value()));
    QCOMPARE(source.notes(), (QList<GhostNote>{
                                 {2280, 240, 65, 6}
    }));
    QVERIFY(scene.runtime().history().undo(scene.command()));
    flushGhostChanges(source);
    QCOMPARE(source.notes(), (QList<GhostNote>{
                                 {480,  960, 60, 4},
                                 {2280, 240, 65, 6}
    }));
    QVERIFY(scene.document.context->m_appModel->findClipById(referenceAId.value()));
    source.setHostClip(scene.referenceB);
    flushGhostChanges(source);
    QCOMPARE(source.notes(), (QList<GhostNote>{
                                 {480,  960, 60, 4},
                                 {1200, 240, 60, 1},
                                 {2400, 120, 72, 1}
    }));
    QVERIFY(scene.runtime().documents().commitNewDocument(
        scene.command(), DocumentAutomationFacade::newDocumentDraft(false)));
    flushGhostChanges(source);
    QVERIFY(!source.enabled());
    QVERIFY(source.notes().isEmpty());
    QCOMPARE(source.maxLength(), 0);
}

void EditorInteractionTests::ghostNoteGeometryFollowsTheVisibleReferenceRange() {
    GhostScene scene;
    scene.initialize();
    if (QTest::currentTestFailed())
        return;
    GhostNoteSource source;
    source.setHostClip(scene.host);
    flushGhostChanges(source);
    GhostNoteLayer layer;
    connect(&source, &GhostNoteSource::changed, &source, [&] { layer.markDirty(); });
    const auto colorA = GhostNoteStyle::fillColor(4);
    const auto colorB = GhostNoteStyle::fillColor(8);
    const auto frame = [&](double from, double to) {
        return layer.ensureUpToDate(&source, scene.host->start(), 16.0, 0.5, 12.0, 1.0, from, to,
                                    700.0, 900.0);
    };
    const auto first = frame(0, 480);
    const auto firstBounds = referenceBounds(first, colorA);
    QVERIFY(!firstBounds.isEmpty());
    QVERIFY(firstBounds.left() < 16.0 && firstBounds.right() > 16.0);
    QVERIFY(std::abs(firstBounds.center().x() - 16.0) < 1e-4);
    QVERIFY(std::abs(firstBounds.center().y() - 810.0) < 1e-4);
    QVERIFY(referenceBounds(first, colorB).isEmpty());
    QCOMPARE(referenceBounds(frame(20, 460), colorA), firstBounds);
    const auto later = frame(800, 1600);
    QVERIFY(referenceBounds(later, colorA).isEmpty());
    const auto laterBounds = referenceBounds(later, colorB);
    QVERIFY(!laterBounds.isEmpty());
    QVERIFY(std::abs(laterBounds.center().x() - 676.0) < 1e-4);
    QVERIFY(std::abs(laterBounds.center().y() - 762.0) < 1e-4);
    const auto before = TestSupport::projectSnapshot(*scene.document.context->m_appModel);
    const auto version = scene.runtime().documentVersion();
    scene.setEnabled(false);
    flushGhostChanges(source);
    QVERIFY(frame(800, 1600).isEmpty());
    scene.setEnabled(true);
    flushGhostChanges(source);
    QCOMPARE(referenceBounds(frame(800, 1600), colorB), laterBounds);
    QCOMPARE(TestSupport::projectSnapshot(*scene.document.context->m_appModel), before);
    QCOMPARE(scene.runtime().documentVersion(), version);
}
