#include "AppContext.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Modules/Audio/AudioContext.h"
#include "TestRuntime.h"

#include <lite/Tasking/TaskManager.h>

#include <QCoreApplication>
#include <QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QFile>
#include <QCryptographicHash>

#include <TalcsFormat/FormatManager.h>

#include <algorithm>

namespace {
    Automation::CoreRuntime *g_runtime = nullptr;
}

template <>
Automation::CoreRuntime *AppContext::instance<Automation::CoreRuntime>() {
    return g_runtime;
}

AudioContext::AudioContext(QObject *parent) : talcs::DspxProjectContext(parent) {
    setFormatManager(new talcs::FormatManager(this));
}

AudioContext::~AudioContext() = default;

AudioContext *AudioContext::instance() {
    static AudioContext context;
    return &context;
}

bool AudioContext::willStartCallback(AudioExporter *) {
    return false;
}

void AudioContext::willFinishCallback(AudioExporter *) {
}

bool DocumentWorkflowController::busy() const {
    return false;
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
            g_runtime = &runtime();
            SingletonRegistry::add(&state.model());
            controller = SingletonRegistry::create<AudioDecodingController>();
            controller->setGuiServices(
                nullptr, [this](const QString &message) { messages.append(message); });
            QObject::connect(&state.model(), &AppModel::modelChanged, controller,
                             &AudioDecodingController::onModelChanged);
            QObject::connect(&state.model(), &AppModel::trackChanged, controller,
                             &AudioDecodingController::onTrackChanged);
            QObject::connect(controller, &AudioDecodingController::resolveSessionFinished,
                             controller,
                             [this](const QList<int> &missing, const QList<int> &unconfirmed, int) {
                                 notifications.append(missing + unconfirmed);
                             });
        }

        ~Fixture() {
            taskManager->terminateAllTasks();
            taskManager->wait();
            drainTasks();
            SingletonRegistry::destroy(controller);
            SingletonRegistry::remove<AppModel>();
            g_runtime = nullptr;
        }

        CoreRuntime &runtime() {
            return state.runtime();
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
            return qobject_cast<AudioClip *>(*state.model().tracks().first()->clips().begin());
        }

        AutomationTestSupport::TestRuntime state;
        QTemporaryDir directory;
        AudioDecodingController *controller = nullptr;
        QList<QList<int>> notifications;
        QStringList messages;
    };

}

class TestAudioDecodingController final : public QObject {
    Q_OBJECT

private slots:

    void openSources_data() {
        QTest::addColumn<int>("invocationSource");
        QTest::newRow("mcp") << int(InvocationSource::PublicMcp);
        QTest::newRow("json-rpc") << int(InvocationSource::PublicJsonRpc);
        QTest::newRow("internal") << int(InvocationSource::InternalAutomation);
        QTest::newRow("gui") << int(InvocationSource::TrustedGui);
    }

    void openSources() {
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
        QVERIFY2((fixture.runtime().documentVersion().revision == 0 &&
                  fixture.state.history()->isOnSavePoint()),
                 "resource checks must preserve revision and save point");
    }

    void replacementBeforeDeferredStart() {
        Fixture fixture;
        const auto guiOpen = fixture.open(InvocationSource::TrustedGui);
        const auto publicOpen = fixture.open(InvocationSource::PublicMcp);
        QVERIFY2((guiOpen && publicOpen), "both replacement requests must commit");
        QVERIFY(drainTasks());
        QVERIFY2((fixture.notifications.isEmpty() &&
                  fixture.firstAudioClip()->pathStatus() == AudioClip::PathStatus::Missing),
                 "a superseded GUI load must not prompt for the new automation document");
    }

    void mixedImportSources() {
        Fixture fixture;
        const auto opened = fixture.open(InvocationSource::PublicMcp);
        QVERIFY2((bool(opened)), "the import target must open");
        QVERIFY(drainTasks());
        for (const auto source : {InvocationSource::PublicJsonRpc, InvocationSource::TrustedGui}) {
            const auto imported = fixture.runtime().documents().commitImportedDocument(
                fixture.command(source),
                missingAudioDocument(fixture.directory.filePath(QStringLiteral("imported.wav"))),
                false, false);
            QVERIFY2((bool(imported)), "both imports must commit");
        }
        auto *guiClip = *fixture.state.model().tracks().last()->clips().begin();
        QVERIFY(drainTasks());
        QVERIFY2((fixture.notifications == QList<QList<int>>{{guiClip->id()}}),
                 "overlapping GUI and automation checks must only prompt for the GUI clip");
    }

    void resolutionRetryPreservesSource() {
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

    void relocatedDecodeNotification() {
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
                    document.tracks.first().clips.first().audioPathInfo.sha512 =
                        QString::fromLatin1(
                            QCryptographicHash::hash(bytes, QCryptographicHash::Sha512).toHex());
                }
                const auto opened = fixture.openDocument(document, source);
                QVERIFY2((bool(opened)), "the relocation fixture must open");
                QVERIFY(drainTasks());
                if (resolution == ResolutionCase::Cascade) {
                    if (!writeCandidate())
                        QVERIFY2((false), "the cascade candidate must be created");
                    const auto cascade =
                        fixture.runtime().dispatcher().dispatchApplicationCommand<int>(
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
                const bool reported = std::any_of(
                    fixture.messages.cbegin(), fixture.messages.cend(),
                    [&](const QString &message) { return message.contains(relocated); });
                QVERIFY2((reported == (source == InvocationSource::TrustedGui)),
                         "decode failures after resolver writeback must retain GUI origin");
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestAudioDecodingController)
#include "main.moc"
