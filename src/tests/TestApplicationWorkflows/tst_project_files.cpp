#include "tst_application_workflows.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/FileAutomationAdapter.h"
#include "Automation/Public/PublicAutomationHostAdapter.h"
#include "Controller/DocumentWorkflow/IProjectLoadSession.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/ProjectFormats/LibreSVIPFormatHandler.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectConverters/LibreSVIPConverter.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <memory>

void ApplicationWorkflowTests::projectBatchImportUsesRealLoaders_data() {
    QTest::addColumn<int>("invalidItems");
    QTest::addColumn<bool>("bestEffort");
    QTest::addColumn<QString>("changeAfterAdmission");
    QTest::newRow("midi-and-dspx") << 0 << false << QString{};
    QTest::newRow("atomic-failure") << 1 << false << QString{};
    QTest::newRow("best-effort") << 1 << true << QString{};
    QTest::newRow("best-effort-all-failed") << 2 << true << QString{};
    QTest::newRow("atomic-source-changed") << 0 << false << QStringLiteral("source");
    QTest::newRow("best-effort-source-changed") << 0 << true << QStringLiteral("source");
    QTest::newRow("document-edited-before-commit") << 0 << false << QStringLiteral("document");
}

void ApplicationWorkflowTests::projectBatchImportUsesRealLoaders() {
    QFETCH(int, invalidItems);
    QFETCH(bool, bestEffort);
    QFETCH(QString, changeAfterAdmission);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto dspx = files.filePath(QStringLiteral("source.dspx"));
    const auto midi = files.filePath(QStringLiteral("source.mid"));
    QString error;
    DspxProjectConverter dspxConverter;
    MidiConverter midiConverter;
    QVERIFY2(dspxConverter.save(dspx, context->m_appModel, error), qPrintable(error));
    QVERIFY2(midiConverter.save(midi, context->m_appModel, error), qPrintable(error));
    if (invalidItems > 0) {
        QFile broken(midi);
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(broken.write("invalid midi"), qint64(12));
    }
    if (invalidItems > 1) {
        QFile broken(dspx);
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(broken.write("invalid project"), qint64(15));
    }
    const auto before = runtime().documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto initialTrackCount = context->m_appModel->tracks().size();
    Automation::AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    Automation::AutomationFileGuard fileGuard;
    Automation::AdmissionController admission;
    QVERIFY(fileGuard.setConfiguredRoots({files.path()}));
    Automation::PublicAutomationRegistry registry(
        runtime(), access, fileGuard, admission,
        Automation::createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                       &SynthrtEngine::instance()));
    QJsonArray items;
    const QStringList paths{dspx, midi};
    for (int index = 0; index < paths.size(); ++index) {
        QJsonObject item{
            {QStringLiteral("path"), paths[index]}
        };
        const bool valid = index == 0 ? invalidItems < 2 : invalidItems == 0;
        if (valid) {
            const auto plan =
                registry.invoke(QStringLiteral("formats.inspect"),
                                {
                                    {QStringLiteral("path"),    paths[index]            },
                                    {QStringLiteral("purpose"), QStringLiteral("import")}
            });
            QVERIFY2(plan, qPrintable(plan ? QString{} : plan.getError().message));
            const auto digest = plan.get().value(QStringLiteral("plan_digest")).toString();
            QVERIFY(!digest.isEmpty());
            item.insert(QStringLiteral("plan_digest"), digest);
        }
        items.append(item);
    }
    const QJsonObject arguments{
        {QStringLiteral("document_id"),       before.documentId.toString()        },
        {QStringLiteral("expected_revision"), static_cast<qint64>(before.revision)},
        {QStringLiteral("failure_policy"),
         bestEffort ? QStringLiteral("best_effort") : QStringLiteral("atomic")    },
        {QStringLiteral("items"),             items                               }
    };
    const auto idFromResult = [](const QJsonObject &result) {
        return Automation::TaskId::fromString(result.value(QStringLiteral("task_id")).toString());
    };
    if (invalidItems == 0 && changeAfterAdmission.isEmpty()) {
        auto previewArguments = arguments;
        previewArguments.insert(QStringLiteral("validate_only"), true);
        const auto tasksBefore = runtime().automationTasks().list(before.documentId);
        const auto preview =
            registry.invoke(QStringLiteral("documents.import_batch"), previewArguments);
        QVERIFY2(preview, qPrintable(preview ? QString{} : preview.getError().message));
        QVERIFY(preview.get().value(QStringLiteral("validated_only")).toBool());
        QCOMPARE(runtime().automationTasks().list(before.documentId), tasksBefore);
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
        const auto pending = registry.invoke(QStringLiteral("documents.import_batch"), arguments);
        QVERIFY2(pending, qPrintable(pending ? QString{} : pending.getError().message));
        const auto pendingId = idFromResult(pending.get());
        QVERIFY(runtime().tasks().cancelTask(commandContext(), pendingId));
        QTRY_COMPARE(runtime().tasks().getTask(before.documentId, pendingId).get().state,
                     Automation::AutomationTaskState::Canceled);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
    }
    const auto accepted = registry.invoke(QStringLiteral("documents.import_batch"), arguments);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto id = idFromResult(accepted.get());
    QVERIFY(!id.isNull());
    if (changeAfterAdmission == QStringLiteral("source")) {
        QFile changed(midi);
        QVERIFY(changed.open(QIODevice::Append));
        QCOMPARE(changed.write("changed after inspection"), qint64(24));
    } else if (changeAfterAdmission == QStringLiteral("document")) {
        QVERIFY(runtime().project().renameTrack(
            commandContext(), Automation::TrackId(context->m_appModel->tracks().first()->id()),
            QStringLiteral("Edited while importing")));
    }
    const auto expectedVersion = runtime().documentVersion();
    const auto expectedModel = context->m_appModel->serialize();
    const auto *expectedUndo = historyManager->nextUndoEntry();
    const auto task = [&] { return runtime().tasks().getTask(before.documentId, id); };
    QTRY_VERIFY_WITH_TIMEOUT(
        task() && (task().get().state == Automation::AutomationTaskState::Succeeded ||
                   task().get().state == Automation::AutomationTaskState::Failed),
        10000);
    if (invalidItems == 2 ||
        ((invalidItems > 0 || !changeAfterAdmission.isEmpty()) && !bestEffort)) {
        QCOMPARE(task().get().state, Automation::AutomationTaskState::Failed);
        if (changeAfterAdmission == QStringLiteral("source")) {
            QVERIFY(task().get().error);
            QCOMPARE(task().get().error->code, Automation::AutomationErrorCode::InvalidArgument);
            QCOMPARE(task().get().error->fieldPath, QStringLiteral("items.plan_digest"));
        } else if (changeAfterAdmission == QStringLiteral("document")) {
            QVERIFY(task().get().error);
            QCOMPARE(task().get().error->code, Automation::AutomationErrorCode::RevisionConflict);
        }
        QCOMPARE(runtime().documentVersion(), expectedVersion);
        QCOMPARE(context->m_appModel->tracks().size(), initialTrackCount);
        QCOMPARE(context->m_appModel->serialize(), expectedModel);
        QCOMPARE(historyManager->nextUndoEntry(), expectedUndo);
    } else {
        const auto terminal = task().get();
        QVERIFY2(terminal.state == Automation::AutomationTaskState::Succeeded,
                 qPrintable(terminal.error ? terminal.error->message : QString{}));
        QVERIFY(context->m_appModel->tracks().size() > initialTrackCount);
        QVERIFY(terminal.mutation);
        QCOMPARE(terminal.mutation->warnings.isEmpty(),
                 invalidItems == 0 && changeAfterAdmission.isEmpty());
        if (changeAfterAdmission == QStringLiteral("source"))
            QCOMPARE(context->m_appModel->tracks().size(), initialTrackCount * 2);
        QCOMPARE(runtime().documentVersion().revision, before.revision + 1);
        QVERIFY(runtime().history().undo(commandContext()));
        QCOMPARE(context->m_appModel->tracks().size(), initialTrackCount);
        QCOMPARE(context->m_appModel->tracks().first()->name(), QStringLiteral("Target"));
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
    }
}

void ApplicationWorkflowTests::publicSaveChecksTheCurrentPathBeforeReplacingTheDocument_data() {
    QTest::addColumn<QString>("requestedName");
    QTest::addColumn<QString>("extensionPolicy");
    QTest::newRow("append-missing-extension")
        << QStringLiteral("current") << QStringLiteral("append_if_missing");
    QTest::newRow("replace-other-extension")
        << QStringLiteral("current.data") << QStringLiteral("replace");
}

void ApplicationWorkflowTests::publicSaveChecksTheCurrentPathBeforeReplacingTheDocument() {
    QFETCH(QString, requestedName);
    QFETCH(QString, extensionPolicy);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto incomingDirectory = files.filePath(QStringLiteral("incoming"));
    QVERIFY(QDir().mkpath(incomingDirectory));
    const auto currentPath = files.filePath(QStringLiteral("current.dspx"));
    const auto requestedPath = files.filePath(requestedName);
    const QByteArray originalFile("Existing unrelated file");
    {
        QFile untouched(requestedPath);
        QVERIFY(untouched.open(QIODevice::WriteOnly));
        QCOMPARE(untouched.write(originalFile), qint64(originalFile.size()));
    }
    const auto incomingPath = QDir(incomingDirectory).filePath(QStringLiteral("replacement.dspx"));
    AppModel replacement;
    replacement.setTimeline(context->m_appModel->timeline());
    auto *replacementTrack = new Track;
    replacementTrack->setName(QStringLiteral("Replacement"));
    QVERIFY(replacement.appendTrack(replacementTrack));
    DspxProjectConverter converter;
    QString error;
    QVERIFY2(converter.save(incomingPath, &replacement, error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    const auto currentTrack = Automation::TrackId(context->m_appModel->tracks().first()->id());
    QVERIFY(
        runtime().project().renameTrack(commandContext(), currentTrack, QStringLiteral("Draft")));
    Automation::AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    Automation::AutomationFileGuard guard;
    Automation::AdmissionController admission;
    QVERIFY(guard.setConfiguredRoots({files.path()}));
    Automation::PublicAutomationRegistry registry(
        runtime(), access, guard, admission,
        Automation::createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                       &SynthrtEngine::instance()));
    const auto saveArguments = [&] {
        const auto version = runtime().documentVersion();
        return QJsonObject{
            {"document_id",       version.documentId.toString()        },
            {"expected_revision", static_cast<qint64>(version.revision)}
        };
    };
    const auto originalVersion = runtime().documentVersion();
    const auto missingPath = registry.invoke(QStringLiteral("documents.save"), saveArguments());
    QVERIFY(!missingPath);
    QCOMPARE(missingPath.getError().code, Automation::AutomationErrorCode::PathRequired);
    QCOMPARE(runtime().documentVersion(), originalVersion);
    auto saveAs = saveArguments();
    saveAs.insert(QStringLiteral("path"), requestedPath);
    saveAs.insert(QStringLiteral("extension_policy"), extensionPolicy);
    saveAs.insert(QStringLiteral("overwrite_policy"), QStringLiteral("reject"));
    auto validationArguments = saveAs;
    validationArguments.insert(QStringLiteral("validate_only"), true);
    const auto validated =
        registry.invoke(QStringLiteral("documents.save_as"), validationArguments);
    QVERIFY2(validated, qPrintable(validated ? QString{} : validated.getError().message));
    QVERIFY(!QFileInfo::exists(currentPath));
    QCOMPARE(runtime().documentVersion(), originalVersion);
    const auto stillUnnamed = runtime().documents().getDocument(originalVersion.documentId);
    QVERIFY(stillUnnamed);
    QVERIFY(stillUnnamed.get().path.isEmpty());
    QVERIFY(!historyManager->isOnSavePoint());
    const auto savedAs = registry.invoke(QStringLiteral("documents.save_as"), saveAs);
    QVERIFY2(savedAs, qPrintable(savedAs ? QString{} : savedAs.getError().message));
    QVERIFY(historyManager->isOnSavePoint());
    const auto named = runtime().documents().getDocument(runtime().documentVersion().documentId);
    QVERIFY(named);
    QCOMPARE(QFileInfo(named.get().path).canonicalFilePath(),
             QFileInfo(currentPath).canonicalFilePath());
    {
        QFile untouched(requestedPath);
        QVERIFY(untouched.open(QIODevice::ReadOnly));
        QCOMPARE(untouched.readAll(), originalFile);
    }
    QVERIFY(runtime().project().renameTrack(commandContext(), currentTrack,
                                            QStringLiteral("Unsaved edit")));
    QVERIFY(!historyManager->isOnSavePoint());
    const auto dirtyVersion = runtime().documentVersion();
    const auto dirtyModel = context->m_appModel->serialize();
    const auto *undo = historyManager->nextUndoEntry();
    const auto open = [&] {
        const auto version = runtime().documentVersion();
        return registry.invoke(QStringLiteral("documents.open"),
                               {
                                   {"current_document_id", version.documentId.toString()        },
                                   {"expected_revision",   static_cast<qint64>(version.revision)},
                                   {"path",                incomingPath                         },
                                   {"unsaved_policy",      "reject"                             }
        });
    };
    const auto dirtyOpen = open();
    QVERIFY(!dirtyOpen);
    QVERIFY2(dirtyOpen.getError().code == Automation::AutomationErrorCode::InvalidArgument,
             qPrintable(dirtyOpen.getError().message + QStringLiteral(" at ") +
                        dirtyOpen.getError().fieldPath));
    QCOMPARE(dirtyOpen.getError().fieldPath, QStringLiteral("unsaved_policy"));
    QVERIFY(guard.setConfiguredRoots({incomingDirectory}));
    const auto denied = registry.invoke(QStringLiteral("documents.save"), saveArguments());
    QVERIFY(!denied);
    QCOMPARE(denied.getError().code, Automation::AutomationErrorCode::PermissionDenied);
    QCOMPARE(runtime().documentVersion(), dirtyVersion);
    QCOMPARE(context->m_appModel->serialize(), dirtyModel);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
    {
        AppModel persisted;
        QVERIFY2(converter.load(currentPath, &persisted, error, ImportMode::NewProject),
                 qPrintable(error));
        QCOMPARE(persisted.tracks().first()->name(), QStringLiteral("Draft"));
    }
    QVERIFY(guard.setConfiguredRoots({files.path()}));
    const auto saved = registry.invoke(QStringLiteral("documents.save"), saveArguments());
    QVERIFY2(saved, qPrintable(saved ? QString{} : saved.getError().message));
    QVERIFY(historyManager->isOnSavePoint());
    QCOMPARE(runtime().documentVersion(), dirtyVersion);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
    {
        AppModel persisted;
        QVERIFY2(converter.load(currentPath, &persisted, error, ImportMode::NewProject),
                 qPrintable(error));
        QCOMPARE(persisted.tracks().first()->name(), QStringLiteral("Unsaved edit"));
    }
    const auto accepted = open();
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto taskId = Automation::TaskId::fromString(accepted.get().value("task_id").toString());
    QVERIFY(!taskId.isNull());
    const auto completed = [&] {
        const auto task = runtime().tasks().getTask(runtime().documentVersion().documentId, taskId);
        return task && (task.get().state == Automation::AutomationTaskState::Succeeded ||
                        task.get().state == Automation::AutomationTaskState::Failed ||
                        task.get().state == Automation::AutomationTaskState::Canceled);
    };
    QTRY_VERIFY_WITH_TIMEOUT(completed(), 10000);
    const auto task = runtime().tasks().getTask(runtime().documentVersion().documentId, taskId);
    QVERIFY(task);
    QVERIFY2(task.get().state == Automation::AutomationTaskState::Succeeded,
             qPrintable(task.get().error ? task.get().error->message : QString{}));
    QVERIFY(runtime().documentVersion().documentId != dirtyVersion.documentId);
    QCOMPARE(context->m_appModel->tracks().size(), 1);
    QCOMPARE(context->m_appModel->tracks().first()->name(), QStringLiteral("Replacement"));
    QVERIFY(historyManager->isOnSavePoint());
    QVERIFY(!historyManager->canUndo());
}

void ApplicationWorkflowTests::publicProjectLoadUsesThePreparedPlan_data() {
    QTest::addColumn<QString>("sourceFormat");
    QTest::addColumn<bool>("opening");
    QTest::addColumn<QByteArray>("changeAfterAdmission");
    QTest::newRow("import-native-dspx") << QStringLiteral("dspx") << false << QByteArray();
    QTest::newRow("import-external-libresvip")
        << QStringLiteral("libresvip") << false << QByteArray();
    QTest::newRow("open-native-dspx") << QStringLiteral("dspx") << true << QByteArray();
    QTest::newRow("open-external-libresvip") << QStringLiteral("libresvip") << true << QByteArray();
    QTest::newRow("import-midi") << QStringLiteral("midi") << false << QByteArray();
    QTest::newRow("open-midi") << QStringLiteral("midi") << true << QByteArray();
    QTest::newRow("cancel-queued-open") << QStringLiteral("dspx") << true << QByteArray("cancel");
    QTest::newRow("revoke-import-access")
        << QStringLiteral("dspx") << false << QByteArray("revoke");
    QTest::newRow("replace-import-source")
        << QStringLiteral("dspx") << false << QByteArray("replace-source");
    QTest::newRow("edit-while-opening")
        << QStringLiteral("dspx") << true << QByteArray("edit-document");
}

void ApplicationWorkflowTests::publicProjectLoadUsesThePreparedPlan() {
    QFETCH(QString, sourceFormat);
    QFETCH(bool, opening);
    QFETCH(QByteArray, changeAfterAdmission);
    const bool externalConverter = sourceFormat == QStringLiteral("libresvip");
    const bool midi = sourceFormat == QStringLiteral("midi");
    const auto oldExecutable = context->m_appOptions->general()->libreSVIPPath;
    const auto oldResult = qgetenv("DSEL_TEST_LIBRESVIP_RESULT");
    const auto restoreConverter = qScopeGuard([&] {
        context->m_appOptions->general()->libreSVIPPath = oldExecutable;
        if (oldResult.isNull())
            qunsetenv("DSEL_TEST_LIBRESVIP_RESULT");
        else
            qputenv("DSEL_TEST_LIBRESVIP_RESULT", oldResult);
    });
    if (externalConverter) {
        context->m_appOptions->general()->libreSVIPPath = QCoreApplication::applicationFilePath();
        qputenv("DSEL_TEST_LIBRESVIP_RESULT", "success");
    }
    QTemporaryDir files;
    QVERIFY(files.isValid());
    AppModel source;
    source.setTimeline(Timeline(
        {
            {0, 87.0}
    },
        {{0, 6, 8}}));
    auto *sourceTrack = new Track;
    sourceTrack->setName(QStringLiteral("Imported lead"));
    auto *sourceClip = new SingingClip;
    sourceClip->setStart(1920);
    sourceClip->setLength(1920);
    sourceClip->setClipLen(1920);
    auto *sourceNote = new Note(sourceClip);
    sourceNote->setLocalStart(240);
    sourceNote->setLength(120);
    sourceNote->setKeyIndex(72);
    sourceNote->setLyric(QStringLiteral("你好"));
    sourceNote->setLanguage(QStringLiteral("cmn"));
    sourceClip->insertNote(sourceNote);
    sourceTrack->insertClip(sourceClip);
    QVERIFY(source.appendTrack(sourceTrack));
    const auto path = files.filePath(midi                ? QStringLiteral("待导入.mid")
                                     : externalConverter ? QStringLiteral("待导入 project.svp")
                                                         : QStringLiteral("待导入.dspx"));
    std::unique_ptr<IProjectConverter> converter;
    if (midi)
        converter = std::make_unique<MidiConverter>();
    else
        converter = std::make_unique<DspxProjectConverter>();
    QString error;
    QVERIFY2(converter->save(path, &source, error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    const auto before = runtime().documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto originalTracks = context->m_appModel->tracks();
    Automation::AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    Automation::AutomationFileGuard fileGuard;
    Automation::AdmissionController admission;
    QVERIFY(fileGuard.setConfiguredRoots({files.path()}));
    Automation::PublicAutomationRegistry registry(
        runtime(), access, fileGuard, admission,
        Automation::createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                       &SynthrtEngine::instance()));
    const auto inspected =
        registry.invoke(QStringLiteral("formats.inspect"),
                        {
                            {QStringLiteral("path"),    path                            },
                            {QStringLiteral("purpose"),
                             opening ? QStringLiteral("open") : QStringLiteral("import")}
    });
    QVERIFY2(inspected, qPrintable(inspected ? QString() : inspected.getError().message));
    const auto digest = inspected.get().value(QStringLiteral("plan_digest")).toString();
    QVERIFY(!digest.isEmpty());
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QJsonObject arguments{
        {opening ? QStringLiteral("current_document_id") : QStringLiteral("document_id"),
         before.documentId.toString()                                                                                         },
        {QStringLiteral("expected_revision"),                                             static_cast<qint64>(before.revision)},
        {QStringLiteral("path"),                                                          path                                },
        {QStringLiteral("options"),                                                       QJsonObject{}                       },
        {QStringLiteral("plan_digest"),                                                   digest                              }
    };
    if (opening)
        arguments.insert(QStringLiteral("unsaved_policy"), QStringLiteral("discard"));
    const auto accepted = registry.invoke(
        opening ? QStringLiteral("documents.open") : QStringLiteral("documents.import"), arguments,
        {.clientId = QStringLiteral("project-import-client"),
         .source = Automation::InvocationSource::PublicJsonRpc});
    QVERIFY2(accepted, qPrintable(accepted ? QString() : accepted.getError().message));
    const auto id =
        Automation::TaskId::fromString(accepted.get().value(QStringLiteral("task_id")).toString());
    QVERIFY(!id.isNull());
    if (changeAfterAdmission == "cancel") {
        QVERIFY(runtime().tasks().cancelTask(commandContext(), id));
    } else if (changeAfterAdmission == "revoke") {
        QVERIFY(fileGuard.setConfiguredRoots({}));
    } else if (changeAfterAdmission == "replace-source") {
        sourceTrack->setName(QStringLiteral("Different source after admission"));
        QVERIFY2(converter->save(path, &source, error), qPrintable(error));
    } else if (changeAfterAdmission == "edit-document") {
        QVERIFY(runtime().project().renameTrack(commandContext(),
                                                Automation::TrackId(originalTracks.first()->id()),
                                                QStringLiteral("Edit while opening")));
    }
    const auto expectedRetainedVersion = runtime().documentVersion();
    const auto expectedRetainedModel = context->m_appModel->serialize();
    const auto task = [&] {
        return runtime().tasks().getTask(runtime().documentVersion().documentId, id);
    };
    QTRY_VERIFY_WITH_TIMEOUT(
        task() && (task().get().state == Automation::AutomationTaskState::Succeeded ||
                   task().get().state == Automation::AutomationTaskState::Failed ||
                   task().get().state == Automation::AutomationTaskState::Canceled),
        10000);
    const auto terminal = task().get();
    if (!changeAfterAdmission.isEmpty()) {
        QCOMPARE(terminal.state, changeAfterAdmission == "cancel"
                                     ? Automation::AutomationTaskState::Canceled
                                     : Automation::AutomationTaskState::Failed);
        if (terminal.state == Automation::AutomationTaskState::Failed)
            QVERIFY(terminal.error && !terminal.error->message.isEmpty());
        QCOMPARE(runtime().documentVersion(), expectedRetainedVersion);
        QCOMPARE(context->m_appModel->serialize(), expectedRetainedModel);
        QCOMPARE(historyManager->canUndo(), changeAfterAdmission == "edit-document");
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        return;
    }
    QVERIFY2(terminal.state == Automation::AutomationTaskState::Succeeded,
             qPrintable(terminal.error ? terminal.error->message : QString()));
    const auto tracks = context->m_appModel->tracks();
    if (opening) {
        QVERIFY(runtime().documentVersion().documentId != before.documentId);
        QCOMPARE(tracks.size(), 1);
        if (midi) {
            QVERIFY(qAbs(context->m_appModel->timeline().tempos().first().value - 87.0) < 0.001);
            QCOMPARE(context->m_appModel->timeline().timeSignatures(),
                     source.timeline().timeSignatures());
        } else {
            QCOMPARE(context->m_appModel->timeline(), source.timeline());
        }
        const auto document =
            runtime().documents().getDocument(runtime().documentVersion().documentId);
        QVERIFY(document);
        if (externalConverter || midi)
            QVERIFY(document.get().path.isEmpty());
        else
            QCOMPARE(QFileInfo(document.get().path).canonicalFilePath(),
                     QFileInfo(path).canonicalFilePath());
        QCOMPARE(historyManager->isOnSavePoint(), !externalConverter && !midi);
        QVERIFY(!historyManager->canUndo());
    } else {
        QCOMPARE(runtime().documentVersion().documentId, before.documentId);
        QCOMPARE(tracks.size(), originalTracks.size() + 1);
        for (int index = 0; index < originalTracks.size(); ++index)
            QCOMPARE(tracks.at(index), originalTracks.at(index));
    }
    const auto *importedTrack = tracks.last();
    QCOMPARE(importedTrack->name(), sourceTrack->name());
    QCOMPARE(importedTrack->clips().count(), 1);
    const auto *importedClip = qobject_cast<SingingClip *>(*importedTrack->clips().begin());
    QVERIFY(importedClip);
    if (!midi)
        QCOMPARE(importedClip->start(), sourceClip->start());
    QCOMPARE(importedClip->notes().count(), 1);
    const auto *importedNote = *importedClip->notes().begin();
    QCOMPARE(importedClip->start() + importedNote->localStart(),
             sourceClip->start() + sourceNote->localStart());
    QCOMPARE(importedNote->length(), sourceNote->length());
    QCOMPARE(importedNote->keyIndex(), sourceNote->keyIndex());
    QCOMPARE(importedNote->lyric(), sourceNote->lyric());
    if (!opening) {
        QVERIFY(runtime().history().undo(commandContext()));
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
        QVERIFY(!historyManager->canUndo());
    }
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
}

void ApplicationWorkflowTests::libreSvipProcessFailuresLeaveTheDocumentUntouched_data() {
    QTest::addColumn<QByteArray>("result");
    QTest::newRow("converter-not-configured") << QByteArray("unconfigured");
    QTest::newRow("converter-cannot-start") << QByteArray("missing-executable");
    QTest::newRow("converter-rejects-source") << QByteArray("error");
    QTest::newRow("converter-omits-output") << QByteArray("missing");
    QTest::newRow("converter-writes-empty-output") << QByteArray("empty");
}

void ApplicationWorkflowTests::libreSvipProcessFailuresLeaveTheDocumentUntouched() {
    QFETCH(QByteArray, result);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto inputPath = files.filePath(QStringLiteral("source.svp"));
    QFile input(inputPath);
    QVERIFY(input.open(QIODevice::WriteOnly));
    QCOMPARE(input.write("fixture input"), 13);
    input.close();
    const auto oldExecutable = context->m_appOptions->general()->libreSVIPPath;
    const auto oldResult = qgetenv("DSEL_TEST_LIBRESVIP_RESULT");
    const auto restoreConverter = qScopeGuard([&] {
        context->m_appOptions->general()->libreSVIPPath = oldExecutable;
        if (oldResult.isNull())
            qunsetenv("DSEL_TEST_LIBRESVIP_RESULT");
        else
            qputenv("DSEL_TEST_LIBRESVIP_RESULT", oldResult);
    });
    context->m_appOptions->general()->libreSVIPPath = QCoreApplication::applicationFilePath();
    qputenv("DSEL_TEST_LIBRESVIP_RESULT", result);
    const auto before = runtime().documentVersion();
    const auto model = context->m_appModel->serialize();
    if (result == "unconfigured" || result == "missing-executable") {
        const auto executable = result == "unconfigured"
                                    ? QString()
                                    : files.filePath(QStringLiteral("missing-converter"));
        const auto failed = LibreSVIPConverter::convertToDspx(executable, inputPath);
        QVERIFY(!failed.success());
        QVERIFY(!failed.errorMessage.isEmpty());
    } else {
        const auto services = Automation::createFileAutomationServices();
        const auto failed = services.convertLibreSvipToDspx(inputPath);
        QVERIFY(!failed);
        QCOMPARE(failed.getError().code, Automation::AutomationErrorCode::FormatUnsupported);
        QVERIFY(!failed.getError().message.isEmpty());
        if (result == "error")
            QVERIFY(
                failed.getError().message.contains(QStringLiteral("Fixture conversion rejected")));
    }
    if (result == "error") {
        Automation::PublicDocumentBatchImportRequest request;
        request.command = commandContext();
        request.items = {
            {.canonicalPath = inputPath, .formatId = QStringLiteral("libresvip")}
        };
        const auto host = Automation::createPublicAutomationHostServices(
            runtime(), context->m_appModel, &SynthrtEngine::instance());
        const auto accepted = host.importDocuments(request);
        QVERIFY2(accepted, qPrintable(accepted ? QString() : accepted.getError().message));
        const auto task = [&] {
            return runtime().tasks().getTask(before.documentId, accepted.get().taskId);
        };
        QTRY_VERIFY_WITH_TIMEOUT(
            task() && task().get().state == Automation::AutomationTaskState::Failed, 10000);
        QVERIFY(task().get().error);
        QVERIFY(
            task().get().error->message.contains(QStringLiteral("Fixture conversion rejected")));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    }
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(context->m_appModel->serialize(), model);
}
