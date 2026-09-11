#include "tst_audio_assets.h"

#include "AppContext.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/DocumentWorkflow/IDocumentWorkflowUi.h"
#include "Controller/Tasks/DecodeAudioTask.h"
#include "Modules/Audio/AudioContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "../TestSupport/RuntimeResourcesFixture.h"
#include "../TestSupport/WaveFixture.h"

#include <lite/Tasking/TaskManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/History/HistoryManager.h>

#include <QCoreApplication>
#include <QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QFile>
#include <QCryptographicHash>
#include <QPointer>
#include <QScopeGuard>
#include <QSemaphore>

#include <TalcsDevice/AbstractOutputContext.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsCore/MixerAudioSource.h>

#include <algorithm>
#include <atomic>
#include <memory>

namespace {
    std::unique_ptr<AppContext> applicationContext;
    std::unique_ptr<QTemporaryDir> applicationData;
    QByteArray previousDataRoot;
}

namespace {
    using namespace Automation;

    bool expect(const bool condition, const char *message) {
        return QTest::qVerify(condition, "task completion", message, __FILE__, __LINE__);
    }

    bool drainTasks() {
        QElapsedTimer timer;
        timer.start();
        do {
            QCoreApplication::processEvents();
            if (taskManager->tasks().isEmpty())
                return true;
            QThread::msleep(1);
        } while (timer.elapsed() < 10000);
        return expect(false, "audio resolution tasks must finish");
    }

    DocumentDraftDto missingAudioDocument(const QString &path) {
        auto document = DocumentAutomationFacade::newDocumentDraft(false);
        TrackDraftDto track;
        track.name = QStringLiteral("Audio");
        ClipDraftDto clip;
        clip.type = ClipDraftDto::Type::Audio;
        clip.properties.name = QStringLiteral("Missing audio");
        clip.properties.length = 480;
        clip.properties.clipLen = 480;
        clip.audioPath = path;
        track.clips.append(clip);
        document.tracks.append(track);
        return document;
    }

    class Fixture final {
    public:
        Fixture() {
            controller = applicationContext->m_audioDecodingController;
            controller->setGuiServices(
                nullptr, [this](const QString &message) { messages.append(message); });
            notificationConnection = QObject::connect(
                controller, &AudioDecodingController::resolveSessionFinished, controller,
                [this](const QList<int> &missing, const QList<int> &unconfirmed, int) {
                    notifications.append(missing + unconfirmed);
                });
        }

        ~Fixture() {
            taskManager->terminateAllTasks();
            taskManager->wait();
            drainTasks();
            QObject::disconnect(notificationConnection);
            controller->setGuiServices(nullptr, {});
            const auto reset = runtime().documents().commitNewDocument(
                command(InvocationSource::InternalAutomation),
                DocumentAutomationFacade::newDocumentDraft(false));
            QTest::qVerify(bool(reset), "document reset", "audio fixture must release its document",
                           __FILE__, __LINE__);
            drainTasks();
        }

        CoreRuntime &runtime() {
            return *applicationContext->m_coreRuntime;
        }

        AppModel &model() {
            return *applicationContext->m_appModel;
        }

        HistoryManager *history() {
            return applicationContext->m_historyManager;
        }

        CommandContext command(const InvocationSource source) {
            return {.expected = runtime().documentVersion(), .source = source};
        }

        AutomationResult<MutationResult> open(const InvocationSource source) {
            return openDocument(
                missingAudioDocument(directory.filePath(QStringLiteral("missing.wav"))), source);
        }

        AutomationResult<MutationResult> openDocument(const DocumentDraftDto &document,
                                                      const InvocationSource source) {
            auto context = command(source);
            const auto admitted = runtime().dispatcher().admitDocumentTask(context);
            if (!admitted)
                return admitted.getError();
            return runtime().documents().commitOpenedDocument(
                context, document, directory.filePath(QStringLiteral("project.dspx")),
                QStringLiteral("project"), true);
        }

        AudioClip *firstAudioClip() {
            return qobject_cast<AudioClip *>(*model().tracks().first()->clips().begin());
        }

        QTemporaryDir directory;
        AudioDecodingController *controller = nullptr;
        QMetaObject::Connection notificationConnection;
        QList<QList<int>> notifications;
        QStringList messages;
    };

    class SaveDecisionUi final : public IDocumentWorkflowUi {
    public:
        QWidget *documentWorkflowParentWidget() override {
            return nullptr;
        }

        SaveDecision askDocumentSaveDecision() override {
            return decide();
        }

        QString chooseDocumentSavePath(const QString &) override {
            return {};
        }

        bool confirmOpenWithoutPackageMetadata() override {
            return false;
        }

        void showDocumentWorkflowError(const ProjectOperationError &error) override {
            errors.append(error);
        }

        void showDocumentWorkflowBusy() override {
            QTest::qFail("Unexpected overlapping document request", __FILE__, __LINE__);
        }

        std::function<SaveDecision()> decide;
        QList<ProjectOperationError> errors;
    };
}

void AudioAssetsTests::initTestCase() {
    applicationData = std::make_unique<QTemporaryDir>();
    QVERIFY(applicationData->isValid());
    previousDataRoot = qgetenv("DSEL_TEST_DATA_ROOT");
    qputenv("DSEL_TEST_DATA_ROOT", applicationData->path().toUtf8());
    QVERIFY(TestSupport::initializeApplicationResources());
    AppEnvironment::postInit(AppHostMode::Headless);
    auto options = std::make_unique<AppOptions>();
    options->general()->packageSearchPaths.clear();
    options->inference()->autoStartInfer = false;
    options->inference()->executionProvider = QStringLiteral("CPU");
    options->inference()->cacheDirectory = applicationData->filePath(QStringLiteral("cache"));
    applicationContext = std::make_unique<AppContext>(std::move(options), AppHostMode::Headless);
    if (auto *device = AudioSystem::outputSystem()->context()->device()) {
        device->stop();
        device->close();
    }
    AudioContext::instance()->preMixer()->close();
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
}

void AudioAssetsTests::cleanupTestCase() {
    applicationContext.reset();
    if (previousDataRoot.isNull())
        qunsetenv("DSEL_TEST_DATA_ROOT");
    else
        qputenv("DSEL_TEST_DATA_ROOT", previousDataRoot);
    applicationData.reset();
}

void AudioAssetsTests::openSources_data() {
    QTest::addColumn<int>("invocationSource");
    QTest::newRow("mcp") << int(InvocationSource::PublicMcp);
    QTest::newRow("json-rpc") << int(InvocationSource::PublicJsonRpc);
    QTest::newRow("internal") << int(InvocationSource::InternalAutomation);
    QTest::newRow("gui") << int(InvocationSource::TrustedGui);
}

void AudioAssetsTests::openSources() {
    QFETCH(int, invocationSource);
    const auto source = static_cast<InvocationSource>(invocationSource);

    Fixture fixture;
    QVERIFY2((fixture.directory.isValid()), "fixture directory must exist");

    fixture.notifications.clear();
    const auto opened = fixture.open(source);
    QVERIFY2((bool(opened)), "project open must commit");
    QVERIFY(drainTasks());
    QVERIFY2((fixture.firstAudioClip()->pathStatus() == AudioClip::PathStatus::Missing),
             "all callers must receive the missing audio status");
    QVERIFY2((fixture.notifications.size() == (source == InvocationSource::TrustedGui)),
             "only an interactive open may request the resource dialog");
    QVERIFY2(
        (fixture.runtime().documentVersion().revision == 0 && fixture.history()->isOnSavePoint()),
        "resource checks must preserve revision and save point");
}

void AudioAssetsTests::replacementBeforeDeferredStart() {
    Fixture fixture;
    const auto guiOpen = fixture.open(InvocationSource::TrustedGui);
    const auto publicOpen = fixture.open(InvocationSource::PublicMcp);
    QVERIFY2((guiOpen && publicOpen), "both replacement requests must commit");
    QVERIFY(drainTasks());
    QVERIFY2((fixture.notifications.isEmpty() &&
              fixture.firstAudioClip()->pathStatus() == AudioClip::PathStatus::Missing),
             "a superseded GUI load must not prompt for the new automation document");
}

void AudioAssetsTests::mixedImportSources() {
    Fixture fixture;
    const auto opened = fixture.open(InvocationSource::PublicMcp);
    QVERIFY2((bool(opened)), "the import target must open");
    QVERIFY(drainTasks());
    for (const auto source : {InvocationSource::PublicJsonRpc, InvocationSource::TrustedGui}) {
        const auto imported = fixture.runtime().documents().commitImportedDocument(
            fixture.command(source),
            missingAudioDocument(fixture.directory.filePath(QStringLiteral("imported.wav"))), false,
            false);
        QVERIFY2((bool(imported)), "both imports must commit");
    }
    auto *guiClip = *fixture.model().tracks().last()->clips().begin();
    QVERIFY(drainTasks());
    QVERIFY2((fixture.notifications == QList<QList<int>>{{guiClip->id()}}),
             "overlapping GUI and automation checks must only prompt for the GUI clip");
}

void AudioAssetsTests::resolutionRetryPreservesSource() {
    Fixture fixture;
    const auto opened = fixture.open(InvocationSource::PublicMcp);
    QVERIFY2((bool(opened)), "the retry fixture must open");
    const auto connection = QObject::connect(
        taskManager, &TaskManager::taskChanged, fixture.controller,
        [&](TaskManager::TaskChangeType type, Task *, qsizetype) {
            if (type != TaskManager::Added)
                return;
            const auto updated = fixture.runtime().dispatcher().dispatchDocumentCommand(
                QStringLiteral("test.change.project.path"),
                fixture.command(InvocationSource::TrustedGui),
                [&](DocumentSession &session, bool) -> AutomationResult<MutationResult> {
                    const auto previous = session.version();
                    session.setPathAndProjectName(
                        fixture.directory.filePath(QStringLiteral("moved/project.dspx")),
                        QStringLiteral("moved project"));
                    return MutationResult{.previous = previous, .current = previous};
                });
            QVERIFY2((bool(updated)), "project path change must commit");
        });
    QVERIFY(drainTasks());
    QObject::disconnect(connection);
    QVERIFY2((fixture.notifications.isEmpty() &&
              fixture.firstAudioClip()->pathStatus() == AudioClip::PathStatus::Missing),
             "a resolution restarted after a path change must remain non-interactive");
}

void AudioAssetsTests::decodeCompletionWaitsForTheSaveDecision_data() {
    QTest::addColumn<bool>("replaceDocument");
    QTest::newRow("cancel-new-applies-waveform-to-original-document") << false;
    QTest::newRow("discard-original-drops-completed-waveform") << true;
}

void AudioAssetsTests::decodeCompletionWaitsForTheSaveDecision() {
    QFETCH(bool, replaceDocument);
    Fixture fixture;
    auto *workflow = DocumentWorkflowController::instance();
    QVERIFY(fixture.directory.isValid());
    const auto path = fixture.directory.filePath(QStringLiteral("pending.wav"));
    QVERIFY(TestSupport::writeWave(path, QVector<float>(4800, 0.25f)));
    QSemaphore entered;
    QSemaphore release;
    std::atomic_bool paused = false;
    QPointer<DecodeAudioTask> decode;
    bool completionDelivered = false;
    TaskId taskId;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *candidate = dynamic_cast<DecodeAudioTask *>(task);
                if (change != TaskManager::Added || !candidate || decode)
                    return;
                decode = candidate;
                taskId = candidate->automationTaskId;
                connect(
                    candidate, &Task::statusUpdated, &observations,
                    [&](const TaskStatus &) {
                        if (!paused.exchange(true)) {
                            entered.release();
                            release.acquire();
                        }
                    },
                    Qt::DirectConnection);
                connect(candidate, &Task::finished, &observations,
                        [&] { completionDelivered = true; });
            });
    const auto releaseOnFailure = qScopeGuard([&] { release.release(); });
    QVERIFY(fixture.openDocument(missingAudioDocument(path), InvocationSource::InternalAutomation));
    QTRY_COMPARE_WITH_TIMEOUT(entered.available(), 1, 5000);
    QVERIFY(decode);
    const QPointer<AudioClip> audio(fixture.firstAudioClip());
    QVERIFY(audio);
    QVERIFY(audio->audioInfo().peakCache.isEmpty());
    QVERIFY(fixture.runtime().project().renameTrack(fixture.command(InvocationSource::TrustedGui),
                                                    TrackId(fixture.model().tracks().first()->id()),
                                                    QStringLiteral("Unsaved edit")));
    const auto before = fixture.runtime().documentVersion();
    const auto *historyBefore = fixture.history()->nextUndoEntry();
    SaveDecisionUi ui;
    workflow->setUi(&ui);
    fixture.controller->setGuiServices(workflow);
    const auto restoreUi = qScopeGuard([&] { workflow->setUi(nullptr); });
    bool prompted = false;
    ui.decide = [&] {
        prompted = true;
        expect(workflow->busy(), "the save decision must keep the document workflow busy");
        release.release();
        const auto completed = QTest::qWaitFor([&] { return completionDelivered; }, 5000);
        expect(completed, "the audio worker must finish during the save decision");
        expect(decode && taskManager->tasks().contains(decode),
               "the completed decode must remain owned until writeback resumes");
        expect(audio && audio->audioInfo().peakCache.isEmpty(),
               "the waveform must not be written while the workflow is busy");
        expect(fixture.runtime().documentVersion() == before &&
                   fixture.history()->nextUndoEntry() == historyBefore,
               "deferred completion must preserve the document and user history");
        return replaceDocument ? SaveDecision::Discard : SaveDecision::Cancel;
    };
    workflow->requestNew();
    QTRY_VERIFY_WITH_TIMEOUT(prompted && !workflow->busy(), 10000);
    QVERIFY(ui.errors.isEmpty());
    QVERIFY(drainTasks());
    QVERIFY(!decode);
    if (replaceDocument) {
        QVERIFY(fixture.runtime().documentVersion().documentId != before.documentId);
        QVERIFY(!audio);
        QVERIFY(fixture.history()->isOnSavePoint());
        QVERIFY(!fixture.history()->canUndo());
    } else {
        QVERIFY(audio);
        QCOMPARE(fixture.runtime().documentVersion(), before);
        QCOMPARE(fixture.history()->nextUndoEntry(), historyBefore);
        QCOMPARE(audio->audioInfo().frames, 4800);
        QVERIFY(!audio->audioInfo().peakCache.isEmpty());
        const auto completed = fixture.runtime().tasks().getTask(before.documentId, taskId);
        QVERIFY(completed);
        QCOMPARE(completed.get().state, AutomationTaskState::Succeeded);
    }
}

void AudioAssetsTests::sourceRemovedBeforeDecodingBecomesMissing() {
    Fixture fixture;
    QVERIFY(fixture.directory.isValid());
    const auto path = fixture.directory.filePath(QStringLiteral("removed-before-read.wav"));
    QVERIFY(TestSupport::writeWave(path, QVector<float>(4800, 0.25f)));
    TaskId taskId;
    QPointer<DecodeAudioTask> decoding;
    bool removed = false;
    QString removalError;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *candidate = dynamic_cast<DecodeAudioTask *>(task);
                if (change != TaskManager::Added || !candidate || !taskId.isNull())
                    return;
                taskId = candidate->automationTaskId;
                decoding = candidate;
                QFile source(path);
                removed = source.remove();
                removalError = source.errorString();
            });
    QVERIFY(fixture.openDocument(missingAudioDocument(path), InvocationSource::InternalAutomation));
    const auto before = fixture.runtime().documentVersion();
    QVERIFY(drainTasks());
    QVERIFY(!taskId.isNull());
#ifdef Q_OS_WIN
    if (!removed)
        QSKIP("The Windows audio source holds the file without delete sharing");
#endif
    QVERIFY2(removed, qPrintable(removalError));
    QVERIFY(!decoding);
    auto *audio = fixture.firstAudioClip();
    QVERIFY(audio);
    QCOMPARE(audio->path(), path);
    QCOMPARE(audio->pathStatus(), AudioClip::PathStatus::Missing);
    QVERIFY(audio->audioInfo().peakCache.isEmpty());
    const auto failed = fixture.runtime().tasks().getTask(before.documentId, taskId);
    QVERIFY(failed);
    QCOMPARE(failed.get().state, AutomationTaskState::Failed);
    QVERIFY(failed.get().error);
    QCOMPARE(failed.get().error->code, AutomationErrorCode::FileNotFound);
    QVERIFY(fixture.history()->isOnSavePoint());
    QVERIFY(!fixture.history()->canUndo());
}

void AudioAssetsTests::relocatedDecodeNotification() {
    enum class ResolutionCase { Candidate, Verified, Cascade };

    for (const auto source : {InvocationSource::TrustedGui, InvocationSource::PublicMcp}) {
        for (const auto resolution :
             {ResolutionCase::Candidate, ResolutionCase::Verified, ResolutionCase::Cascade}) {
            Fixture fixture;
            const QByteArray bytes("invalid audio content");
            const auto relocated = fixture.directory.filePath(QStringLiteral("invalid.wav"));
            const auto writeCandidate = [&] {
                QFile file(relocated);
                return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
            };
            if (resolution != ResolutionCase::Cascade && !writeCandidate())
                QVERIFY2((false), "the undecodable candidate must be created");
            auto document = missingAudioDocument(
                fixture.directory.filePath(QStringLiteral("gone/invalid.wav")));
            if (resolution != ResolutionCase::Candidate) {
                document.tracks.first().clips.first().audioPathInfo.sha512 = QString::fromLatin1(
                    QCryptographicHash::hash(bytes, QCryptographicHash::Sha512).toHex());
            }
            const auto opened = fixture.openDocument(document, source);
            QVERIFY2((bool(opened)), "the relocation fixture must open");
            QVERIFY(drainTasks());
            if (resolution == ResolutionCase::Cascade) {
                if (!writeCandidate())
                    QVERIFY2((false), "the cascade candidate must be created");
                const auto cascade = fixture.runtime().dispatcher().dispatchApplicationCommand<int>(
                    QStringLiteral("test.cascade"), {.source = source},
                    [&](bool) -> AutomationResult<int> {
                        fixture.controller->resolveMissingClipsNear(relocated);
                        return 0;
                    });
                QVERIFY2((bool(cascade)), "cascade resolution must start");
                QVERIFY(drainTasks());
            }
            QVERIFY2((fixture.firstAudioClip()->path() == relocated),
                     "the candidate must be adopted before decoding");
            const bool reported =
                std::any_of(fixture.messages.cbegin(), fixture.messages.cend(),
                            [&](const QString &message) { return message.contains(relocated); });
            QVERIFY2((reported == (source == InvocationSource::TrustedGui)),
                     "decode failures after resolver writeback must retain GUI origin");
        }
    }
}
