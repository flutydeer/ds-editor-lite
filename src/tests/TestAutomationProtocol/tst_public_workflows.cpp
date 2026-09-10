#include "tst_automation_protocol.h"

#include "Automation/Public/AdmissionController.h"
#include "Automation/Public/AutomationAccessPolicy.h"
#include "Automation/Public/AutomationFileGuard.h"
#include "Automation/Public/PublicAutomationRegistry.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"
#include "TestRuntime.h"

#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>

using namespace Automation;

namespace {
    FileRuntimeServices projectFormats() {
        FileRuntimeServices services;
        services.listProjectFormats = [] {
            return QList<ProjectFormatDto>{
                {.id = QStringLiteral("dspx"),
                 .displayName = QStringLiteral("DSPX"),
                 .extensions = {QStringLiteral("*.dspx")},
                 .canOpen = true,
                 .canImport = true},
                {.id = QStringLiteral("midi"),
                 .displayName = QStringLiteral("MIDI"),
                 .extensions = {QStringLiteral("*.mid")},
                 .canOpen = true,
                 .canImport = true},
            };
        };
        return services;
    }

    QJsonObject commandArguments(const DocumentVersion &version) {
        return {{QStringLiteral("document_id"), version.documentId.toString()},
                {QStringLiteral("expected_revision"), static_cast<qint64>(version.revision)}};
    }

    CommandContext commandContext(CoreRuntime &runtime) {
        return {.expected = runtime.documentVersion(), .source = InvocationSource::Test};
    }

    QString errorMessage(const AutomationResult<QJsonObject> &result) {
        return result ? QString() : result.getError().fieldPath + QStringLiteral(": ") +
                                         result.getError().message;
    }

    bool writeFile(const QString &path, const QByteArray &bytes) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               file.write(bytes) == bytes.size();
    }

    TrackDraftDto lyricTrack() {
        ClipDraftDto clip;
        clip.type = ClipDraftDto::Type::Singing;
        clip.properties.name = QStringLiteral("Phrase");
        clip.properties.length = 3840;
        clip.properties.clipLen = 3840;
        clip.defaultLanguage = QStringLiteral("cmn");
        const QStringList lyrics{QStringLiteral("old-a"), QStringLiteral("-"),
                                 QStringLiteral("old-b"), QStringLiteral("old-c")};
        for (qsizetype index = 0; index < lyrics.size(); ++index) {
            NoteDraftDto note;
            note.localStart = static_cast<int>(index) * 480;
            note.length = 240;
            note.keyIndex = 60;
            note.lyric = lyrics.at(index);
            note.language = QStringLiteral("eng");
            clip.notes.append(note);
        }
        TrackDraftDto track;
        track.name = QStringLiteral("Lead");
        track.defaultLanguage = QStringLiteral("cmn");
        track.singerInfo = SingerInfo(
            {QStringLiteral("fixture"), QStringLiteral("fixture-package"), QVersionNumber(1, 0)},
            QStringLiteral("Fixture"), {},
            {LanguageInfo(QStringLiteral("cmn"), QStringLiteral("Chinese"), QStringLiteral("g2p")),
             LanguageInfo(QStringLiteral("eng"), QStringLiteral("English"), QStringLiteral("g2p"))},
            QStringLiteral("cmn"));
        track.clips.append(clip);
        return track;
    }

    class RegistryFixture {
    public:
        RegistryFixture()
            : runtimeFixture({}, {}, projectFormats()),
              runtime(runtimeFixture.runtime()), access(AutomationWire::ControlLevel::L3) {
            runtimeFixture.model().newProject();
        }

        PublicAutomationHostServices captureBatch() {
            PublicAutomationHostServices services;
            services.importDocuments = [this](const PublicDocumentBatchImportRequest &request) {
                batches.append(request);
                return AutomationResult<TaskAcceptedResult>(
                    TaskAcceptedResult{{}, request.command.expected, true});
            };
            return services;
        }

        AutomationTestSupport::TestRuntime runtimeFixture;
        CoreRuntime &runtime;
        AutomationAccessPolicy access;
        AutomationFileGuard fileGuard;
        AdmissionController admission;
        QList<PublicDocumentBatchImportRequest> batches;
    };
}

void AutomationProtocolTests::batchImportRouting_data() {
    QTest::addColumn<QString>("policy");
    QTest::newRow("atomic") << QStringLiteral("atomic");
    QTest::newRow("best-effort") << QStringLiteral("best_effort");
}

void AutomationProtocolTests::batchImportRouting() {
    QFETCH(QString, policy);
    RegistryFixture fixture;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(fixture.fileGuard.setConfiguredRoots({directory.path()}));
    const auto midiPath = directory.filePath(QStringLiteral("phrase.mid"));
    const auto projectPath = directory.filePath(QStringLiteral("phrase.dspx"));
    QVERIFY(writeFile(midiPath, QByteArrayLiteral("deferred MIDI input")));
    QVERIFY(writeFile(projectPath, QByteArrayLiteral("deferred project input")));
    PublicAutomationRegistry registry(fixture.runtime, fixture.access, fixture.fileGuard,
                                      fixture.admission, fixture.captureBatch());
    const auto before = fixture.runtime.documentVersion();
    const auto beforeModel = fixture.runtimeFixture.model().serialize();
    auto arguments = commandArguments(before);
    arguments.insert(QStringLiteral("validate_only"), true);
    arguments.insert(QStringLiteral("failure_policy"), policy);
    arguments.insert(QStringLiteral("items"), QJsonArray{
        QJsonObject{{QStringLiteral("path"), midiPath},
                    {QStringLiteral("options"), QJsonObject{
                         {QStringLiteral("encoding"), QStringLiteral("utf-8")},
                         {QStringLiteral("import_tempo"), false},
                         {QStringLiteral("import_time_signatures"), false}}}},
        QJsonObject{{QStringLiteral("path"), midiPath},
                    {QStringLiteral("format_id"), QStringLiteral("dspx")}},
        QJsonObject{{QStringLiteral("path"), projectPath},
                    {QStringLiteral("options"), QJsonObject{
                         {QStringLiteral("encoding"), QStringLiteral("UTF-8")}}}},
    });
    const auto result = registry.invoke(QStringLiteral("documents.import_batch"), arguments,
                                        {.clientId = QStringLiteral("batch-client"),
                                         .source = InvocationSource::PublicJsonRpc});
    QVERIFY2(result, qPrintable(errorMessage(result)));
    QCOMPARE(fixture.batches.size(), 1);
    const auto &request = fixture.batches.first();
    QCOMPARE(request.failurePolicy, policy == QStringLiteral("atomic")
                                        ? PublicBatchFailurePolicy::Atomic
                                        : PublicBatchFailurePolicy::BestEffort);
    QCOMPARE(request.command.expected, before);
    QVERIFY(request.command.validateOnly);
    QCOMPARE(request.command.clientId, QStringLiteral("batch-client"));
    QCOMPARE(request.command.source, InvocationSource::PublicJsonRpc);
    QCOMPARE(request.items.size(), 3);
    const auto &valid = request.items.at(0);
    QCOMPARE(valid.canonicalPath, QFileInfo(midiPath).canonicalFilePath());
    QCOMPARE(valid.formatId, QStringLiteral("midi"));
    QCOMPARE(valid.options.value(QStringLiteral("encoding")).toString().compare(
                 QStringLiteral("UTF-8"), Qt::CaseInsensitive), 0);
    QCOMPARE(valid.options.value(QStringLiteral("import_tempo")).toBool(), false);
    QCOMPARE(valid.options.value(QStringLiteral("import_time_signatures")).toBool(), false);
    QVERIFY(!valid.validationError);
    QVERIFY(valid.revalidatePlan);
    const auto authorized = valid.revalidatePlan();
    QVERIFY(authorized);
    QVERIFY(!authorized.get());
    QVERIFY(request.items.at(1).validationError);
    QCOMPARE(request.items.at(1).validationError->fieldPath, QStringLiteral("items.format_id"));
    QVERIFY(request.items.at(2).validationError);
    QCOMPARE(request.items.at(2).validationError->fieldPath,
             QStringLiteral("items.options.encoding"));

    QTemporaryDir outside;
    QVERIFY(outside.isValid());
    const auto outsidePath = outside.filePath(QStringLiteral("private.dspx"));
    QVERIFY(writeFile(outsidePath, QByteArrayLiteral("private")));
    arguments.insert(QStringLiteral("items"), QJsonArray{
        QJsonObject{{QStringLiteral("path"), midiPath}},
        QJsonObject{{QStringLiteral("path"), outsidePath}},
    });
    const auto restricted = registry.invoke(QStringLiteral("documents.import_batch"), arguments);
    if (policy == QStringLiteral("atomic")) {
        QVERIFY(!restricted);
        QCOMPARE(restricted.getError().code, AutomationErrorCode::PermissionDenied);
        QCOMPARE(fixture.batches.size(), 1);
    } else {
        QVERIFY2(restricted, qPrintable(errorMessage(restricted)));
        QCOMPARE(fixture.batches.size(), 2);
        const auto &items = fixture.batches.last().items;
        QVERIFY(!items.first().validationError);
        QVERIFY(items.last().validationError);
        QCOMPARE(items.last().validationError->code, AutomationErrorCode::PermissionDenied);
        QCOMPARE(items.last().validationError->fieldPath, QStringLiteral("items[1].path"));
        QVERIFY(items.last().canonicalPath.isEmpty());
    }
    QCOMPARE(fixture.runtime.documentVersion(), before);
    QCOMPARE(fixture.runtimeFixture.model().serialize(), beforeModel);
    QVERIFY(!fixture.runtimeFixture.history()->canUndo());
}

void AutomationProtocolTests::batchImportPlanRevalidation() {
    RegistryFixture fixture;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(fixture.fileGuard.setConfiguredRoots({directory.path()}));
    QVERIFY(fixture.runtime.project().insertTrack(commandContext(fixture.runtime), 0,
                                                  lyricTrack()));
    const auto projectPath = directory.filePath(QStringLiteral("planned.dspx"));
    QString failure;
    DspxProjectConverter converter;
    QVERIFY2(converter.save(projectPath, &fixture.runtimeFixture.model(), failure),
             qPrintable(failure));
    PublicAutomationRegistry registry(fixture.runtime, fixture.access, fixture.fileGuard,
                                      fixture.admission, fixture.captureBatch());
    const auto before = fixture.runtime.documentVersion();
    const auto beforeModel = fixture.runtimeFixture.model().serialize();
    const auto *beforeUndo = fixture.runtimeFixture.history()->nextUndoEntry();
    const auto inspection = registry.invoke(
        QStringLiteral("formats.inspect"),
        {{QStringLiteral("path"), projectPath},
         {QStringLiteral("purpose"), QStringLiteral("import")}});
    QVERIFY2(inspection, qPrintable(errorMessage(inspection)));
    QCOMPARE(inspection.get().value(QStringLiteral("format_id")).toString(),
             QStringLiteral("dspx"));
    QCOMPARE(inspection.get().value(QStringLiteral("sources")).toArray().first().toObject()
                 .value(QStringLiteral("name")).toString(), QStringLiteral("Lead"));
    QCOMPARE(inspection.get().value(QStringLiteral("lyrics_preview")).toArray(),
             (QJsonArray{QStringLiteral("old-a"), QStringLiteral("-"), QStringLiteral("old-b"),
                         QStringLiteral("old-c")}));
    const auto digest = inspection.get().value(QStringLiteral("plan_digest")).toString();
    QVERIFY(!digest.isEmpty());
    auto arguments = commandArguments(before);
    arguments.insert(QStringLiteral("validate_only"), true);
    arguments.insert(QStringLiteral("failure_policy"), QStringLiteral("atomic"));
    arguments.insert(QStringLiteral("items"), QJsonArray{QJsonObject{
        {QStringLiteral("path"), projectPath}, {QStringLiteral("plan_digest"), digest}}});
    const auto accepted = registry.invoke(QStringLiteral("documents.import_batch"), arguments);
    QVERIFY2(accepted, qPrintable(errorMessage(accepted)));
    QCOMPARE(fixture.batches.size(), 1);
    const auto originalItem = fixture.batches.first().items.first();
    QVERIFY(!originalItem.validationError);
    QVERIFY(originalItem.revalidatePlan);
    const auto snapshot = originalItem.revalidatePlan();
    QVERIFY(snapshot);
    QVERIFY(snapshot.get());
    QFile original(projectPath);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(*snapshot.get(), original.readAll());
    original.close();

    // A valid source edit must invalidate the inspected bytes even if its preview is unchanged.
    QVERIFY(writeFile(projectPath, *snapshot.get() + QByteArrayLiteral("\n")));
    const auto stale = originalItem.revalidatePlan();
    QVERIFY(!stale);
    QCOMPARE(stale.getError().code, AutomationErrorCode::InvalidArgument);
    QCOMPARE(stale.getError().fieldPath, QStringLiteral("items.plan_digest"));
    const auto staleRequest = registry.invoke(QStringLiteral("documents.import_batch"), arguments);
    QVERIFY2(staleRequest, qPrintable(errorMessage(staleRequest)));
    QVERIFY(fixture.batches.last().items.first().validationError);
    QCOMPARE(fixture.batches.last().items.first().validationError->fieldPath,
             QStringLiteral("items.plan_digest"));

    const auto inspectedAgain = registry.invoke(
        QStringLiteral("formats.inspect"),
        {{QStringLiteral("path"), projectPath},
         {QStringLiteral("purpose"), QStringLiteral("import")}});
    QVERIFY2(inspectedAgain, qPrintable(errorMessage(inspectedAgain)));
    QVERIFY(inspectedAgain.get().value(QStringLiteral("plan_digest")).toString() != digest);
    arguments.insert(QStringLiteral("items"), QJsonArray{QJsonObject{
        {QStringLiteral("path"), projectPath},
        {QStringLiteral("plan_digest"),
         inspectedAgain.get().value(QStringLiteral("plan_digest"))}}});
    const auto renewed = registry.invoke(QStringLiteral("documents.import_batch"), arguments);
    QVERIFY2(renewed, qPrintable(errorMessage(renewed)));
    const auto renewedItem = fixture.batches.last().items.first();
    QVERIFY(!renewedItem.validationError);
    QTemporaryDir revoked;
    QVERIFY(revoked.isValid());
    QVERIFY(fixture.fileGuard.setConfiguredRoots({revoked.path()}));
    const auto denied = renewedItem.revalidatePlan();
    QVERIFY(!denied);
    QCOMPARE(denied.getError().code, AutomationErrorCode::PermissionDenied);
    QCOMPARE(denied.getError().fieldPath, QStringLiteral("items.path"));
    QCOMPARE(fixture.runtime.documentVersion(), before);
    QCOMPARE(fixture.runtimeFixture.model().serialize(), beforeModel);
    QCOMPARE(fixture.runtimeFixture.history()->nextUndoEntry(), beforeUndo);
}

void AutomationProtocolTests::fillLyricsOptions_data() {
    QTest::addColumn<QString>("splitter");
    QTest::addColumn<QString>("languageMode");
    QTest::addColumn<bool>("skipSlur");
    QTest::newRow("character-explicit-skip-slur")
        << QStringLiteral("character") << QStringLiteral("explicit") << true;
    QTest::newRow("auto-follow-singer")
        << QStringLiteral("auto") << QStringLiteral("follow_singer") << false;
}

void AutomationProtocolTests::fillLyricsOptions() {
    QFETCH(QString, splitter);
    QFETCH(QString, languageMode);
    QFETCH(bool, skipSlur);
    const auto configs = QFINDTESTDATA("../../app/Modules/FillLyric/configs");
    QVERIFY(!configs.isEmpty());
    const auto root = std::filesystem::path(configs.toStdU16String());
    QVERIFY(FillLyric::TextSplitter::init(root / "splitter"));
    QVERIFY(FillLyric::TextTagger::init(root / "tagger", root / "tagger"));
    RegistryFixture fixture;
    QVERIFY(fixture.runtime.project().insertTrack(commandContext(fixture.runtime), 0,
                                                  lyricTrack()));
    const auto project =
        fixture.runtime.project().getProject(fixture.runtime.documentVersion().documentId);
    QVERIFY(project);
    const auto clip = project.get().tracks.first().clips.first();
    const auto originalNotes =
        fixture.runtime.notes().getNotes(project.get().document.documentId, clip.id);
    QVERIFY(originalNotes);
    const auto before = fixture.runtime.documentVersion();
    const auto *beforeUndo = fixture.runtimeFixture.history()->nextUndoEntry();
    QJsonArray noteIds;
    for (auto it = originalNotes.get().crbegin(); it != originalNotes.get().crend(); ++it)
        noteIds.append(it->id.value());
    QJsonObject language{{QStringLiteral("mode"), languageMode}};
    if (languageMode == QStringLiteral("explicit"))
        language.insert(QStringLiteral("language_id"), QStringLiteral("cmn"));
    auto arguments = commandArguments(before);
    arguments.insert(QStringLiteral("clip_id"), clip.id.value());
    arguments.insert(QStringLiteral("note_ids"), noteIds);
    arguments.insert(QStringLiteral("text"), QStringLiteral("你好"));
    arguments.insert(QStringLiteral("options"), QJsonObject{
        {QStringLiteral("splitter_id"), splitter}, {QStringLiteral("skip_slur"), skipSlur},
        {QStringLiteral("language"), language}});
    PublicAutomationRegistry registry(fixture.runtime, fixture.access, fixture.fileGuard,
                                      fixture.admission);
    const auto result = registry.invoke(QStringLiteral("notes.fill_lyrics"), arguments);
    QVERIFY2(result, qPrintable(errorMessage(result)));
    QCOMPARE(fixture.runtime.documentVersion().revision, before.revision + 1);
    const auto notes = fixture.runtime.notes().getNotes(before.documentId, clip.id);
    QVERIFY(notes);
    QCOMPARE(notes.get().at(0).data.lyric, QStringLiteral("你"));
    QCOMPARE(notes.get().at(skipSlur ? 2 : 1).data.lyric, QStringLiteral("好"));
    QCOMPARE(notes.get().at(0).data.language,
             languageMode == QStringLiteral("explicit") ? QStringLiteral("cmn") : QString());
    QCOMPARE(notes.get().at(skipSlur ? 2 : 1).data.language, notes.get().at(0).data.language);
    QCOMPARE(notes.get().at(skipSlur ? 1 : 2).data.lyric,
             originalNotes.get().at(skipSlur ? 1 : 2).data.lyric);
    QCOMPARE(notes.get().at(3).data.lyric, QStringLiteral("old-c"));
    QVERIFY(fixture.runtime.history().undo(commandContext(fixture.runtime)));
    QCOMPARE(fixture.runtimeFixture.history()->nextUndoEntry(), beforeUndo);
    const auto restored = fixture.runtime.notes().getNotes(before.documentId, clip.id);
    QVERIFY(restored);
    for (qsizetype index = 0; index < originalNotes.get().size(); ++index) {
        QCOMPARE(restored.get().at(index).data.lyric, originalNotes.get().at(index).data.lyric);
        QCOMPARE(restored.get().at(index).data.language,
                 originalNotes.get().at(index).data.language);
    }
}

void AutomationProtocolTests::fillLyricsUnavailableLanguage() {
    RegistryFixture fixture;
    QVERIFY(fixture.runtime.project().insertTrack(commandContext(fixture.runtime), 0,
                                                  lyricTrack()));
    const auto project =
        fixture.runtime.project().getProject(fixture.runtime.documentVersion().documentId);
    QVERIFY(project);
    const auto clip = project.get().tracks.first().clips.first();
    const auto originalNotes =
        fixture.runtime.notes().getNotes(project.get().document.documentId, clip.id);
    QVERIFY(originalNotes);
    const auto before = fixture.runtime.documentVersion();
    const auto beforeModel = fixture.runtimeFixture.model().serialize();
    const auto *beforeUndo = fixture.runtimeFixture.history()->nextUndoEntry();
    auto arguments = commandArguments(before);
    arguments.insert(QStringLiteral("clip_id"), clip.id.value());
    arguments.insert(QStringLiteral("note_ids"),
                     QJsonArray{originalNotes.get().first().id.value()});
    arguments.insert(QStringLiteral("text"), QStringLiteral("你好"));
    arguments.insert(QStringLiteral("options"), QJsonObject{
        {QStringLiteral("language"), QJsonObject{
            {QStringLiteral("mode"), QStringLiteral("explicit")},
            {QStringLiteral("language_id"), QStringLiteral("jpn")}}}});
    PublicAutomationRegistry registry(fixture.runtime, fixture.access, fixture.fileGuard,
                                      fixture.admission);
    const auto result = registry.invoke(QStringLiteral("notes.fill_lyrics"), arguments);
    QVERIFY(!result);
    QCOMPARE(result.getError().code, AutomationErrorCode::InvalidArgument);
    QCOMPARE(result.getError().fieldPath, QStringLiteral("/options/language/language_id"));
    QCOMPARE(fixture.runtime.documentVersion(), before);
    QCOMPARE(fixture.runtimeFixture.model().serialize(), beforeModel);
    QCOMPARE(fixture.runtimeFixture.history()->nextUndoEntry(), beforeUndo);
}
