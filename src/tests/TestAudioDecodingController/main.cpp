#include "AppContext.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Modules/Audio/AudioContext.h"
#include "TestRuntime.h"

#include <lite/Tasking/TaskManager.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

namespace {
    Automation::CoreRuntime *g_runtime = nullptr;
}

template <>
Automation::CoreRuntime *AppContext::instance<Automation::CoreRuntime>() {
    return g_runtime;
}

AudioContext *AudioContext::instance() {
    qFatal("Missing-audio fixtures must not request an audio decoder");
    return nullptr;
}

bool DocumentWorkflowController::busy() const {
    return false;
}

namespace {
    using namespace Automation;

    bool expect(const bool condition, const char *message) {
        if (!condition)
            QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return condition;
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
            auto context = command(source);
            const auto admitted = runtime().dispatcher().admitDocumentTask(context);
            if (!admitted)
                return admitted.getError();
            return runtime().documents().commitOpenedDocument(
                context, missingAudioDocument(directory.filePath(QStringLiteral("missing.wav"))),
                directory.filePath(QStringLiteral("project.dspx")), QStringLiteral("project"),
                true);
        }

        AudioClip *firstAudioClip() {
            return qobject_cast<AudioClip *>(*state.model().tracks().first()->clips().begin());
        }

        AutomationTestSupport::TestRuntime state;
        QTemporaryDir directory;
        AudioDecodingController *controller = nullptr;
        QList<QList<int>> notifications;
    };

    bool testOpenSources() {
        Fixture fixture;
        bool ok = expect(fixture.directory.isValid(), "fixture directory must exist");
        for (const auto source :
             {InvocationSource::PublicMcp, InvocationSource::PublicJsonRpc,
              InvocationSource::InternalAutomation, InvocationSource::TrustedGui}) {
            fixture.notifications.clear();
            const auto opened = fixture.open(source);
            ok &= expect(bool(opened), "project open must commit");
            ok &= drainTasks();
            ok &= expect(fixture.firstAudioClip()->pathStatus() == AudioClip::PathStatus::Missing,
                         "all callers must receive the missing audio status");
            ok &= expect(fixture.notifications.size() == (source == InvocationSource::TrustedGui),
                         "only an interactive open may request the resource dialog");
            ok &= expect(fixture.runtime().documentVersion().revision == 0 &&
                             fixture.state.history()->isOnSavePoint(),
                         "resource checks must preserve revision and save point");
        }
        return ok;
    }

    bool testReplacementBeforeDeferredStart() {
        Fixture fixture;
        const auto guiOpen = fixture.open(InvocationSource::TrustedGui);
        const auto publicOpen = fixture.open(InvocationSource::PublicMcp);
        bool ok = expect(guiOpen && publicOpen, "both replacement requests must commit");
        ok &= drainTasks();
        ok &= expect(fixture.notifications.isEmpty() &&
                         fixture.firstAudioClip()->pathStatus() == AudioClip::PathStatus::Missing,
                     "a superseded GUI load must not prompt for the new automation document");
        return ok;
    }

    bool testMixedImportSources() {
        Fixture fixture;
        const auto opened = fixture.open(InvocationSource::PublicMcp);
        bool ok = expect(bool(opened), "the import target must open");
        ok &= drainTasks();
        for (const auto source : {InvocationSource::PublicJsonRpc, InvocationSource::TrustedGui}) {
            const auto imported = fixture.runtime().documents().commitImportedDocument(
                fixture.command(source),
                missingAudioDocument(fixture.directory.filePath(QStringLiteral("imported.wav"))),
                false, false);
            ok &= expect(bool(imported), "both imports must commit");
        }
        auto *guiClip = *fixture.state.model().tracks().last()->clips().begin();
        ok &= drainTasks();
        ok &= expect(fixture.notifications == QList<QList<int>>{{guiClip->id()}},
                     "overlapping GUI and automation checks must only prompt for the GUI clip");
        return ok;
    }

    bool testResolutionRetryPreservesSource() {
        Fixture fixture;
        const auto opened = fixture.open(InvocationSource::PublicMcp);
        bool ok = expect(bool(opened), "the retry fixture must open");
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
                ok &= expect(bool(updated), "project path change must commit");
            });
        ok &= drainTasks();
        QObject::disconnect(connection);
        ok &= expect(fixture.notifications.isEmpty() &&
                         fixture.firstAudioClip()->pathStatus() == AudioClip::PathStatus::Missing,
                     "a resolution restarted after a path change must remain non-interactive");
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = testOpenSources();
    ok &= testReplacementBeforeDeferredStart();
    ok &= testMixedImportSources();
    ok &= testResolutionRetryPreservesSource();
    return ok ? 0 : 1;
}
