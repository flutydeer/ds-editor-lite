#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppDataPaths.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/Utils/InferenceApplyGate.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>

#include <QtTest/QTest>
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include <memory>

namespace {
    using InferenceApplyGate::Decision;

    void addInferenceStages() {
        QTest::addColumn<QString>("stage");
        QTest::newRow("duration") << QStringLiteral("duration");
        QTest::newRow("pitch") << QStringLiteral("pitch");
        QTest::newRow("variance") << QStringLiteral("variance");
        QTest::newRow("acoustic") << QStringLiteral("acoustic");
    }

    InferenceTaskContext captureTask(const QString &stage, const InferPiece &piece) {
        const auto singer = piece.clip->singerIdentifier();
        // Construct the same task snapshot used by completion handlers without scheduling it.
        if (stage == QStringLiteral("duration")) {
            const InferDurationTask task(InferControllerHelper::buildInferDurInput(piece, singer));
            return task.inferenceContext();
        }
        if (stage == QStringLiteral("pitch")) {
            const InferPitchTask task(InferControllerHelper::buildInferPitchInput(piece, singer));
            return task.inferenceContext();
        }
        if (stage == QStringLiteral("variance")) {
            const InferVarianceTask task(
                InferControllerHelper::buildInferVarianceInput(piece, singer));
            return task.inferenceContext();
        }
        const InferAcousticTask task(InferControllerHelper::buildInferAcousticInput(piece, singer));
        return task.inferenceContext();
    }

    Automation::TrackDraftDto trackDraft(const QString &name) {
        Automation::NoteDraftDto note;
        note.localStart = 480;
        note.length = 480;
        note.keyIndex = 60;
        note.lyric = QStringLiteral("a");
        note.language = QStringLiteral("eng");
        PhonemeName phoneme;
        phoneme.language = QStringLiteral("eng");
        phoneme.name = QStringLiteral("a");
        note.phonemes.nameSeq.original = {phoneme};
        note.phonemes.offsetSeq.original = {0};

        Automation::ClipDraftDto clip;
        clip.properties.name = name;
        clip.properties.length = 1920;
        clip.properties.clipLen = 1920;
        clip.defaultLanguage = QStringLiteral("eng");
        clip.notes = {note};

        Automation::TrackDraftDto track;
        track.name = name;
        track.defaultLanguage = QStringLiteral("eng");
        track.clips = {clip};
        return track;
    }
}

class InferenceWorkflowTests final : public QObject {
    Q_OBJECT

private slots:

    void initTestCase() {
        QVERIFY(dataRoot.isValid());
        previousDataRoot = qgetenv("DSEL_TEST_DATA_ROOT");
        qputenv("DSEL_TEST_DATA_ROOT", dataRoot.path().toUtf8());
        dataRootInstalled = true;
        QCOMPARE(AppDataPaths::testRoot(), QDir::cleanPath(dataRoot.path()));
        AppEnvironment::postInit(AppHostMode::Headless);

        auto options = std::make_unique<AppOptions>();
        QVERIFY(QDir::cleanPath(options->configPath()).startsWith(dataRoot.path() + '/'));
        options->general()->packageSearchPaths.clear();
        options->general()->defaultSingingLanguage = QStringLiteral("eng");
        options->inference()->autoStartInfer = false;
        options->inference()->executionProvider = QStringLiteral("CPU");
        options->inference()->cacheDirectory = dataRoot.filePath(QStringLiteral("cache"));
        options->audio()->obj.insert(QStringLiteral("driverName"),
                                     QStringLiteral("inference-test-no-audio-driver"));
        options->audio()->obj.insert(QStringLiteral("deviceName"),
                                     QStringLiteral("inference-test-no-audio-device"));
        context = std::make_unique<AppContext>(std::move(options), AppHostMode::Headless);
    }

    void init() {
        auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
        document.tracks = {trackDraft(QStringLiteral("Target")),
                           trackDraft(QStringLiteral("Other track"))};
        QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
        QCOMPARE(context->m_appModel->tracks().size(), 2);
        const auto *track = context->m_appModel->tracks().first();
        trackId = Automation::TrackId(track->id());
        clip = dynamic_cast<SingingClip *>(*track->clips().begin());
        otherClip =
            dynamic_cast<SingingClip *>(*context->m_appModel->tracks().last()->clips().begin());
        QVERIFY(clip);
        QVERIFY(otherClip);
        clip->reSegment(context->m_appModel->timeline());
        QCOMPARE(clip->pieces().size(), 1);
        piece = clip->pieces().first();
        QCOMPARE(piece->notes.size(), 1);
        note = piece->notes.first();
        QVERIFY(!editSessionManager->hasActiveTransaction());
    }

    void changedTargetInputDropsResult_data() {
        addInferenceStages();
    }

    void changedTargetInputDropsResult() {
        QFETCH(QString, stage);
        const auto snapshot = captureTask(stage, *piece);
        QVERIFY(!snapshot.documentVersion.documentId.isNull());
        QVERIFY(!snapshot.inputSignature.isEmpty());
        QVERIFY(snapshot.taskId >= 0);
        InferenceTaskResolution resolution;
        QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Apply);
        QCOMPARE(resolution.clip, clip);
        QCOMPARE(resolution.piece, piece);
        QCOMPARE(resolution.notes, QList<Note *>({note}));

        // The semantic check must reject changed input even before revision bookkeeping advances.
        note->setKeyIndex(note->keyIndex() + 1);
        QCOMPARE(clip->inferenceRevision(), snapshot.clipRevision);
        QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Drop);
        QCOMPARE(resolution.dropReason, QStringLiteral("input-signature-mismatch"));
        QVERIFY(!resolution.clip);
        QVERIFY(!resolution.piece);
        QVERIFY(resolution.notes.isEmpty());
    }

    void removedTargetDropsResult_data() {
        QTest::addColumn<bool>("replaceDocument");
        QTest::newRow("replace-document") << true;
        QTest::newRow("remove-clip") << false;
    }

    void removedTargetDropsResult() {
        QFETCH(bool, replaceDocument);
        const auto snapshot = captureTask(QStringLiteral("acoustic"), *piece);
        InferenceTaskResolution resolution;
        QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Apply);
        if (replaceDocument) {
            QVERIFY(runtime().documents().commitNewDocument(
                commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
            QVERIFY(runtime().documentVersion().documentId != snapshot.documentVersion.documentId);
        } else {
            QVERIFY(runtime().project().removeClips(commandContext(),
                                                    {Automation::ClipId(snapshot.clipId)}));
            QVERIFY(!context->m_appModel->findClipById(snapshot.clipId));
        }
        QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Drop);
        QCOMPARE(resolution.dropReason, replaceDocument ? QStringLiteral("document-changed")
                                                        : QStringLiteral("clip-not-found"));
        QVERIFY(!resolution.clip);
        QVERIFY(!resolution.piece);
        QVERIFY(resolution.notes.isEmpty());
    }

    void unchangedInputSurvivesRevisionDrift_data() {
        addInferenceStages();
    }

    void unchangedInputSurvivesRevisionDrift() {
        QFETCH(QString, stage);
        const auto snapshot = captureTask(stage, *piece);
        QVERIFY(runtime().project().renameTrack(commandContext(), trackId,
                                                QStringLiteral("Renamed while inference runs")));
        QVERIFY(runtime().documentVersion().revision > snapshot.documentVersion.revision);
        QCOMPARE(runtime().documentVersion().documentId, snapshot.documentVersion.documentId);

        // Re-segmenting unchanged notes advances the clip revision while preserving this piece.
        clip->reSegment(context->m_appModel->timeline());
        QCOMPARE(clip->findPieceById(snapshot.pieceId), piece);
        QVERIFY(clip->inferenceRevision() > snapshot.clipRevision);
        QCOMPARE(InferControllerHelper::buildSemanticSignature(stage, *piece, snapshot.singer),
                 snapshot.inputSignature);
        InferenceTaskResolution resolution;
        QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Apply);
        QCOMPARE(resolution.clip, clip);
        QCOMPARE(resolution.piece, piece);
        QCOMPARE(resolution.notes, QList<Note *>({note}));
        QVERIFY(resolution.dropReason.isEmpty());
    }

    void editSessionControlsResultDeferral_data() {
        using Domain = AppStatus::EditObjectType;
        QTest::addColumn<QString>("stage");
        QTest::addColumn<int>("domain");
        QTest::addColumn<QString>("scope");
        QTest::addColumn<bool>("deferred");
        QTest::newRow("same-note")
            << QStringLiteral("acoustic") << int(Domain::Note) << QStringLiteral("note") << true;
        QTest::newRow("other-clip") << QStringLiteral("acoustic") << int(Domain::Note)
                                    << QStringLiteral("other-clip") << false;
        QTest::newRow("piece-phonemes") << QStringLiteral("duration") << int(Domain::Phoneme)
                                        << QStringLiteral("piece") << true;
        QTest::newRow("whole-clip")
            << QStringLiteral("acoustic") << int(Domain::Clip) << QStringLiteral("clip") << true;
        QTest::newRow("pitch-affects-acoustic")
            << QStringLiteral("acoustic") << int(Domain::Param) << QStringLiteral("pitch") << true;
        QTest::newRow("pitch-does-not-affect-duration")
            << QStringLiteral("duration") << int(Domain::Param) << QStringLiteral("pitch") << false;
    }

    void editSessionControlsResultDeferral() {
        QFETCH(QString, stage);
        QFETCH(int, domain);
        QFETCH(QString, scope);
        QFETCH(bool, deferred);
        const auto snapshot = captureTask(stage, *piece);
        EditSession session;
        session.domain = static_cast<AppStatus::EditObjectType>(domain);
        session.clipId = scope == QStringLiteral("other-clip") ? otherClip->id() : clip->id();
        if (scope == QStringLiteral("note"))
            session.noteIds = {note->id()};
        else if (scope == QStringLiteral("other-clip"))
            session.noteIds = {(*otherClip->notes().begin())->id()};
        else if (scope == QStringLiteral("piece"))
            session.pieceIds = {piece->id()};
        else if (scope == QStringLiteral("pitch"))
            session.params = {ParamInfo::Pitch};
        const auto sessionId = editSessionManager->beginTransaction(session);
        QVERIFY(sessionId != 0);

        InferenceApplyGate::Options options;
        options.checkEditSession = true;
        options.expectedNoteCount = snapshot.noteIds.size();
        InferenceTaskResolution resolution;
        QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution, options),
                 deferred ? Decision::Defer : Decision::Apply);
        QCOMPARE(resolution.clip, clip);
        QCOMPARE(resolution.piece, piece);
        QCOMPARE(resolution.notes, QList<Note *>({note}));
        QCOMPARE(resolution.dropReason,
                 deferred ? QStringLiteral("edit-session-conflict") : QString());
        QCOMPARE(runtime().documentVersion(), snapshot.documentVersion);

        editSessionManager->endTransaction(sessionId, EditSessionEndReason::Discard);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution, options), Decision::Apply);
        QVERIFY(resolution.dropReason.isEmpty());
    }

    void cleanup() {
        if (context)
            editSessionManager->clear();
        clip = nullptr;
        otherClip = nullptr;
        piece = nullptr;
        note = nullptr;
    }

    void cleanupTestCase() {
        context.reset();
        if (dataRootInstalled) {
            if (previousDataRoot.isNull())
                qunsetenv("DSEL_TEST_DATA_ROOT");
            else
                qputenv("DSEL_TEST_DATA_ROOT", previousDataRoot);
        }
    }

private:
    Automation::CoreRuntime &runtime() {
        return *context->m_coreRuntime;
    }

    Automation::CommandContext commandContext() {
        return {.expected = runtime().documentVersion(),
                .source = Automation::InvocationSource::InternalAutomation};
    }

    QTemporaryDir dataRoot;
    QByteArray previousDataRoot;
    bool dataRootInstalled = false;
    std::unique_ptr<AppContext> context;
    Automation::TrackId trackId;
    SingingClip *clip = nullptr;
    SingingClip *otherClip = nullptr;
    InferPiece *piece = nullptr;
    Note *note = nullptr;
};

QTEST_GUILESS_MAIN(InferenceWorkflowTests)
#include "main.moc"
