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
    const auto beforeSearch = fixture.runtime.documentVersion();
    const auto found = registry.invoke(QStringLiteral("notes.search"),
                                       {
                                           {"document_id", before.documentId.toString()},
                                           {"clip_id",     clip.id.value()             },
                                           {"query",       QStringLiteral("好")        },
                                           {"mode",        "exact"                     }
    });
    QVERIFY2(found, qPrintable(errorMessage(found)));
    const auto matches = found.get().value(QStringLiteral("matches")).toArray();
    QCOMPARE(matches.size(), 1);
    const auto match = matches.first().toObject();
    QCOMPARE(match.value(QStringLiteral("note_id")).toInt(),
             notes.get().at(skipSlur ? 2 : 1).id.value());
    QCOMPARE(match.value(QStringLiteral("local_start")).toInt(),
             notes.get().at(skipSlur ? 2 : 1).data.localStart);
    QCOMPARE(fixture.runtime.documentVersion(), beforeSearch);
    auto languageArguments = commandArguments(beforeSearch);
    languageArguments.insert(QStringLiteral("clip_id"), clip.id.value());
    languageArguments.insert(QStringLiteral("note_ids"),
                             QJsonArray{match.value(QStringLiteral("note_id"))});
    languageArguments.insert(QStringLiteral("language"),
                             QJsonObject{
                                 {"mode",        "explicit"},
                                 {"language_id", "eng"     }
    });
    const auto changedLanguage =
        registry.invoke(QStringLiteral("notes.set_language"), languageArguments);
    QVERIFY2(changedLanguage, qPrintable(errorMessage(changedLanguage)));
    const auto afterLanguage = fixture.runtime.notes().getNotes(before.documentId, clip.id);
    QVERIFY(afterLanguage);
    QCOMPARE(afterLanguage.get().at(skipSlur ? 2 : 1).data.language, QStringLiteral("eng"));
    QCOMPARE(afterLanguage.get().first().data.language, notes.get().first().data.language);
    QCOMPARE(afterLanguage.get().at(skipSlur ? 2 : 1).data.lyric, QStringLiteral("好"));
    QVERIFY(fixture.runtime.history().undo(commandContext(fixture.runtime)));
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
    QVERIFY(
        fixture.runtime.project().insertTrack(commandContext(fixture.runtime), 0, lyricTrack()));
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

void AutomationProtocolTests::parameterQueryBoundsSamplesAndPreservesAnchors() {
    RegistryFixture fixture;
    auto &runtime = fixture.runtime;
    QVERIFY(runtime.project().insertTrack(commandContext(runtime), 0, lyricTrack()));
    const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
    QVERIFY(project);
    const auto clip = project.get().tracks.first().clips.first().id;
    CurveDraftDto first;
    first.values = {6000, 6010, 6020, 6030, 6040, 6050, 6060, 6070};
    CurveDraftDto second;
    second.localStart = 100;
    second.values = {6100, 6110, 6120, 6130};
    CurveDraftDto anchor;
    anchor.type = CurveDraftDto::Type::Anchor;
    anchor.nodes = {
        {20, 6200, AnchorNode::Linear },
        {40, 6400, AnchorNode::Hermite}
    };
    auto outside = anchor;
    outside.nodes = {
        {200, 6500, AnchorNode::Linear },
        {240, 6600, AnchorNode::Hermite}
    };
    QVERIFY(runtime.parameters().replaceParameter(commandContext(runtime), clip, ParamInfo::Pitch,
                                                  Param::Edited, {first, second, anchor, outside}));
    const auto stored = runtime.parameters().getParameter(runtime.documentVersion().documentId,
                                                          clip, ParamInfo::Pitch, Param::Edited);
    QVERIFY(stored);
    const auto anchorId = stored.get().curves.at(2).id.value();
    const auto before = runtime.documentVersion();
    const auto beforeModel = fixture.runtimeFixture.model().serialize();
    const auto *beforeUndo = fixture.runtimeFixture.history()->nextUndoEntry();
    PublicAutomationRegistry registry(runtime, fixture.access, fixture.fileGuard,
                                      fixture.admission);
    QJsonObject arguments{
        {QStringLiteral("document_id"), before.documentId.toString()             },
        {QStringLiteral("clip_id"),     clip.value()                             },
        {QStringLiteral("name"),        QStringLiteral("pitch")                  },
        {QStringLiteral("layer"),       QStringLiteral("edited")                 },
        {QStringLiteral("range"),
         QJsonObject{{QStringLiteral("start"), 10}, {QStringLiteral("end"), 125}}},
        {QStringLiteral("max_points"),  6                                        },
    };
    const auto queried = registry.invoke(QStringLiteral("parameters.get"), arguments);
    QVERIFY2(queried, qPrintable(errorMessage(queried)));
    const auto snapshot = queried.get().value(QStringLiteral("snapshot")).toObject();
    QCOMPARE(snapshot.value(QStringLiteral("source_point_count")).toInt(), 12);
    QCOMPARE(snapshot.value(QStringLiteral("returned_point_count")).toInt(), 6);
    QVERIFY(snapshot.value(QStringLiteral("downsampled")).toBool());
    const auto curves = snapshot.value(QStringLiteral("curves")).toArray();
    QCOMPARE(curves.size(), 3);
    const auto firstDraw = curves.at(0).toObject();
    QCOMPARE(firstDraw.value(QStringLiteral("local_start")).toInt(), 10);
    QCOMPARE(firstDraw.value(QStringLiteral("step")).toInt(), 15);
    QCOMPARE(firstDraw.value(QStringLiteral("values")).toArray(), (QJsonArray{6020, 6050}));
    const auto secondDraw = curves.at(1).toObject();
    QCOMPARE(secondDraw.value(QStringLiteral("local_start")).toInt(), 100);
    QCOMPARE(secondDraw.value(QStringLiteral("step")).toInt(), 10);
    QCOMPARE(secondDraw.value(QStringLiteral("values")).toArray(), (QJsonArray{6100, 6120}));
    const auto exactAnchor = curves.at(2).toObject();
    QCOMPARE(exactAnchor.value(QStringLiteral("curve_id")).toInt(), anchorId);
    const auto nodes = exactAnchor.value(QStringLiteral("nodes")).toArray();
    QCOMPARE(nodes.size(), 2);
    QCOMPARE(nodes.first().toObject().value(QStringLiteral("position")).toInt(), 20);
    QCOMPARE(nodes.last().toObject().value(QStringLiteral("position")).toInt(), 40);
    QCOMPARE(nodes.last().toObject().value(QStringLiteral("value")).toInt(), 6400);

    for (int budget : {1, 3}) {
        arguments.insert(QStringLiteral("max_points"), budget);
        const auto insufficient = registry.invoke(QStringLiteral("parameters.get"), arguments);
        QVERIFY(!insufficient);
        QCOMPARE(insufficient.getError().code, AutomationErrorCode::InvalidArgument);
        QCOMPARE(insufficient.getError().fieldPath, QStringLiteral("max_points"));
    }
    arguments.remove(QStringLiteral("max_points"));
    arguments.remove(QStringLiteral("range"));
    const auto complete = registry.invoke(QStringLiteral("parameters.get"), arguments);
    QVERIFY2(complete, qPrintable(errorMessage(complete)));
    const auto completeSnapshot = complete.get().value(QStringLiteral("snapshot")).toObject();
    QVERIFY(!completeSnapshot.value(QStringLiteral("downsampled")).toBool());
    QCOMPARE(completeSnapshot.value(QStringLiteral("returned_point_count")).toInt(), 16);
    QCOMPARE(completeSnapshot.value(QStringLiteral("curves"))
                 .toArray()
                 .first()
                 .toObject()
                 .value(QStringLiteral("values"))
                 .toArray(),
             (QJsonArray{6000, 6010, 6020, 6030, 6040, 6050, 6060, 6070}));
    arguments.insert(QStringLiteral("range"),
                     QJsonObject{
                         {QStringLiteral("start"), 135},
                         {QStringLiteral("end"),   190}
    });
    const auto empty = registry.invoke(QStringLiteral("parameters.get"), arguments);
    QVERIFY2(empty, qPrintable(errorMessage(empty)));
    QVERIFY(empty.get()
                .value(QStringLiteral("snapshot"))
                .toObject()
                .value(QStringLiteral("curves"))
                .toArray()
                .isEmpty());
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(fixture.runtimeFixture.model().serialize(), beforeModel);
    QCOMPARE(fixture.runtimeFixture.history()->nextUndoEntry(), beforeUndo);
}

void AutomationProtocolTests::publicParameterEditsPreserveCurvesAndUndo() {
    RegistryFixture fixture;
    auto &runtime = fixture.runtime;
    QVERIFY(runtime.project().insertTrack(commandContext(runtime), 0, lyricTrack()));
    const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
    QVERIFY(project);
    const auto clip = project.get().tracks.first().clips.first().id;
    PublicAutomationRegistry registry(runtime, fixture.access, fixture.fileGuard,
                                      fixture.admission);
    QJsonObject draw{
        {"type",        "draw"                      },
        {"local_start", 20                          },
        {"step",        10                          },
        {"values",      QJsonArray{6000, 6010, 6020}}
    };
    QJsonObject firstNode{
        {"position",      480     },
        {"value",         6400    },
        {"interpolation", "linear"}
    };
    QJsonObject lastNode{
        {"position",      960      },
        {"value",         6600     },
        {"interpolation", "hermite"}
    };
    QJsonObject anchor{
        {"type",  "anchor"                       },
        {"nodes", QJsonArray{firstNode, lastNode}}
    };
    const auto edit = [&](const QString &tool, QJsonObject arguments) {
        const auto command = commandArguments(runtime.documentVersion());
        for (auto it = command.begin(); it != command.end(); ++it)
            arguments.insert(it.key(), it.value());
        arguments.insert(QStringLiteral("clip_id"), clip.value());
        arguments.insert(QStringLiteral("name"), QStringLiteral("pitch"));
        return registry.invoke(tool, arguments);
    };
    const auto replace = [&](const QJsonArray &curves) {
        return edit(QStringLiteral("parameters.replace"), {
                                                              {"curves", curves}
        });
    };
    fixture.runtimeFixture.history()->reset();
    const auto replaced = replace({draw, anchor});
    QVERIFY2(replaced, qPrintable(errorMessage(replaced)));
    const auto initial = runtime.parameters().getParameter(runtime.documentVersion().documentId,
                                                           clip, ParamInfo::Pitch, Param::Edited);
    QVERIFY(initial);
    QCOMPARE(initial.get().curves.size(), 2);
    const auto &storedDraw = initial.get().curves.first();
    const auto &storedAnchor = initial.get().curves.last();
    QCOMPARE(storedDraw.type, CurveDraftDto::Type::Draw);
    QCOMPARE(storedDraw.localStart, 20);
    QCOMPARE(storedDraw.step, 10);
    QCOMPARE(storedDraw.values, (QList<int>{6000, 6010, 6020}));
    QCOMPARE(storedAnchor.type, CurveDraftDto::Type::Anchor);
    QCOMPARE(storedAnchor.nodes.size(), 2);
    QCOMPARE(storedAnchor.nodes.first().position, 480);
    QCOMPARE(storedAnchor.nodes.first().value, 6400);
    QCOMPARE(storedAnchor.nodes.first().interpolation, AnchorNode::Linear);
    QCOMPARE(storedAnchor.nodes.last().position, 960);
    QCOMPARE(storedAnchor.nodes.last().interpolation, AnchorNode::Hermite);
    QVERIFY(storedDraw.id.isValid() && storedAnchor.id.isValid());
    QVERIFY(storedAnchor.nodes.first().id.isValid() && storedAnchor.nodes.last().id.isValid());
    draw.insert(QStringLiteral("values"), QJsonArray{6100, 6110, 6120});
    lastNode.insert(QStringLiteral("value"), 6700);
    anchor.insert(QStringLiteral("nodes"), QJsonArray{firstNode, lastNode});
    const auto updated = replace({draw, anchor});
    QVERIFY2(updated, qPrintable(errorMessage(updated)));
    const auto edited = runtime.parameters().getParameter(runtime.documentVersion().documentId,
                                                          clip, ParamInfo::Pitch, Param::Edited);
    QVERIFY(edited);
    QCOMPARE(edited.get().curves.size(), 2);
    QCOMPARE(edited.get().curves.first().values, (QList<int>{6100, 6110, 6120}));
    QCOMPARE(edited.get().curves.last().nodes.last().value, 6700);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    const auto restored = runtime.parameters().getParameter(runtime.documentVersion().documentId,
                                                            clip, ParamInfo::Pitch, Param::Edited);
    QVERIFY(restored);
    QCOMPARE(restored.get().curves.size(), 2);
    QCOMPARE(restored.get().curves.first().id, storedDraw.id);
    QCOMPARE(restored.get().curves.first().values, storedDraw.values);
    QCOMPARE(restored.get().curves.last().id, storedAnchor.id);
    QCOMPARE(restored.get().curves.last().nodes.last().id, storedAnchor.nodes.last().id);
    QCOMPARE(restored.get().curves.last().nodes.last().value, storedAnchor.nodes.last().value);

    const auto beforeAnchors = fixture.runtimeFixture.model().serialize();
    const auto versionBeforeAnchors = runtime.documentVersion();
    const auto created = edit(
        QStringLiteral("parameters.create_anchor_curve"),
        {
            {"client_ref", "continuation"                                                    },
            {"anchors",
             QJsonArray{
                 QJsonObject{{"position", 1440}, {"value", 6500}, {"interpolation", "linear"}},
                 QJsonObject{{"position", 1920}, {"value", 6400}, {"interpolation", "step"}}}}
    });
    QVERIFY2(created, qPrintable(errorMessage(created)));
    int curveId = -1;
    for (const auto &value : created.get().value(QStringLiteral("created_objects")).toArray()) {
        const auto entry = value.toObject();
        if (entry.value(QStringLiteral("client_ref")).toString() == QStringLiteral("continuation"))
            curveId = entry.value(QStringLiteral("object"))
                          .toObject()
                          .value(QStringLiteral("id"))
                          .toInt(-1);
    }
    QVERIFY(curveId >= 0);
    const auto inserted =
        edit(QStringLiteral("parameters.insert_anchors"),
             {
                 {"curve_id", curveId                                                                        },
                 {"anchors",  QJsonArray{QJsonObject{{"position", 1560}, {"value", 6700}},
                                        QJsonObject{{"position", 1680},
                                                    {"value", 6600},
                                                    {"interpolation", "linear"}}}}
    });
    QVERIFY2(inserted, qPrintable(errorMessage(inserted)));
    const auto withAnchors = runtime.parameters().getParameter(
        runtime.documentVersion().documentId, clip, ParamInfo::Pitch, Param::Edited);
    QVERIFY(withAnchors);
    QCOMPARE(withAnchors.get().curves.size(), 3);
    const auto &continuation = withAnchors.get().curves.last();
    QCOMPARE(continuation.id.value(), curveId);
    QCOMPARE(continuation.nodes.size(), 4);
    const auto firstInsertedId = continuation.nodes.at(1).id;
    const auto secondInsertedId = continuation.nodes.at(2).id;
    QCOMPARE(continuation.nodes.at(1).position, 1560);
    QCOMPARE(continuation.nodes.at(1).interpolation, AnchorNode::Hermite);
    QCOMPARE(continuation.nodes.at(2).position, 1680);
    QCOMPARE(continuation.nodes.at(2).interpolation, AnchorNode::Linear);
    const auto moved =
        edit(QStringLiteral("parameters.move_anchors"),
             {
                 {"moves", QJsonArray{QJsonObject{{"anchor_id", firstInsertedId.value()},
                                                  {"position", 1500},
                                                  {"value", 6750}},
                                      QJsonObject{{"anchor_id", secondInsertedId.value()},
                                                  {"position", 1740},
                                                  {"value", 6650}}}}
    });
    QVERIFY2(moved, qPrintable(errorMessage(moved)));
    const auto afterMove = runtime.parameters().getParameter(runtime.documentVersion().documentId,
                                                             clip, ParamInfo::Pitch, Param::Edited);
    QVERIFY(afterMove);
    const auto &movedCurve = afterMove.get().curves.last();
    QCOMPARE(movedCurve.id, continuation.id);
    QCOMPARE(movedCurve.nodes.at(1).id, firstInsertedId);
    QCOMPARE(movedCurve.nodes.at(1).position, 1500);
    QCOMPARE(movedCurve.nodes.at(1).value, 6750);
    QCOMPARE(movedCurve.nodes.at(2).id, secondInsertedId);
    QCOMPARE(movedCurve.nodes.at(2).position, 1740);
    QCOMPARE(movedCurve.nodes.at(2).value, 6650);
    QCOMPARE(afterMove.get().curves.first().values, storedDraw.values);
    QCOMPARE(afterMove.get().curves.at(1).id, storedAnchor.id);
    QCOMPARE(runtime.documentVersion().revision, versionBeforeAnchors.revision + 3);
    for (int i = 0; i < 3; ++i)
        QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(fixture.runtimeFixture.model().serialize(), beforeAnchors);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    const auto empty = runtime.parameters().getParameter(runtime.documentVersion().documentId, clip,
                                                         ParamInfo::Pitch, Param::Edited);
    QVERIFY(empty && empty.get().curves.isEmpty());
    QVERIFY(!fixture.runtimeFixture.history()->canUndo());
}

void AutomationProtocolTests::phonemeNamesUseTheEffectiveLanguageAndResetOffsets_data() {
    QTest::addColumn<bool>("inherit");
    QTest::newRow("explicit-note-language") << false;
    QTest::newRow("inherited-clip-language") << true;
}

void AutomationProtocolTests::phonemeNamesUseTheEffectiveLanguageAndResetOffsets() {
    QFETCH(bool, inherit);
    RegistryFixture fixture;
    auto &runtime = fixture.runtime;
    auto track = lyricTrack();
    auto &draft = track.clips.first().notes.first();
    if (inherit)
        draft.language.clear();
    const auto expectedLanguage = inherit ? QStringLiteral("cmn") : QStringLiteral("eng");
    PhonemeName onset;
    onset.name = QStringLiteral("l");
    onset.language = expectedLanguage;
    PhonemeName vowel;
    vowel.name = QStringLiteral("a");
    vowel.language = expectedLanguage;
    draft.phonemes.nameSeq.original = {onset, vowel};
    draft.phonemes.offsetSeq.original = {0, 50};
    draft.phonemes.offsetSeq.edited = {0, 80};
    QVERIFY(runtime.project().insertTrack(commandContext(runtime), 0, track));
    const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
    QVERIFY(project);
    const auto clip = project.get().tracks.first().clips.first().id;
    const auto notes = runtime.notes().getNotes(runtime.documentVersion().documentId, clip);
    QVERIFY(notes && !notes.get().isEmpty());
    const auto original = notes.get().first();
    QVERIFY(!original.data.phonemes.offsetSeq.edited.isEmpty());
    PublicAutomationRegistry registry(runtime, fixture.access, fixture.fileGuard,
                                      fixture.admission);
    auto arguments = commandArguments(runtime.documentVersion());
    arguments.insert(QStringLiteral("clip_id"), clip.value());
    arguments.insert(QStringLiteral("note_id"), original.id.value());
    arguments.insert(QStringLiteral("names"), QJsonArray{"m", "a", "n"});
    fixture.runtimeFixture.history()->reset();
    const auto set = registry.invoke(QStringLiteral("notes.set_phonemes"), arguments);
    QVERIFY2(set, qPrintable(errorMessage(set)));
    const auto after = runtime.notes().getNotes(runtime.documentVersion().documentId, clip);
    QVERIFY(after);
    const auto changed = after.get().first();
    QCOMPARE(changed.id, original.id);
    QCOMPARE(changed.data.language, draft.language);
    QCOMPARE(changed.data.lyric, draft.lyric);
    QCOMPARE(changed.data.phonemes.nameSeq.original, original.data.phonemes.nameSeq.original);
    QVERIFY(changed.data.phonemes.offsetSeq.edited.isEmpty());
    QStringList names;
    for (const auto &phoneme : changed.data.phonemes.nameSeq.edited) {
        names.append(phoneme.name);
        QCOMPARE(phoneme.language, expectedLanguage);
    }
    QCOMPARE(names, (QStringList{QStringLiteral("m"), QStringLiteral("a"), QStringLiteral("n")}));
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    const auto undone = runtime.notes().getNotes(runtime.documentVersion().documentId, clip);
    QVERIFY(undone);
    QCOMPARE(undone.get().first().data.phonemes.nameSeq.edited,
             original.data.phonemes.nameSeq.edited);
    QCOMPARE(undone.get().first().data.phonemes.offsetSeq.edited,
             original.data.phonemes.offsetSeq.edited);
    QVERIFY(!fixture.runtimeFixture.history()->canUndo());
}
