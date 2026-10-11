#include "MidiExtractController.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "ExtractionErrorDialog.h"

#include <lite/ProjectModel/AppModel/AudioClip.h>

namespace {
    Automation::CoreRuntime *automationRuntime() {
        return AppContext::instance<Automation::CoreRuntime>();
    }

    void showExtractionError(const QString &message) {
        Extractors::showErrorDialog(MidiExtractController::tr("Task Failed"), MidiExtractController::tr("Close"), message);
    }
}

MidiExtractController::MidiExtractController(QObject *parent) : ModelChangeHandler(parent) {
}

MidiExtractController::~MidiExtractController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(MidiExtractController)

void MidiExtractController::runExtractMidi(const AudioClip *audioClip) {
    auto *runtime = automationRuntime();
    if (!runtime || !audioClip)
        return;

    const auto path = audioClip->path();
    Automation::ExtractionObserver observer;
    observer.finished = [path](const Automation::AutomationTaskSnapshot &task) {
        if (task.state != Automation::AutomationTaskState::Failed || !task.error)
            return;
        showExtractionError(
            MidiExtractController::tr("Failed to extract MIDI from audio:\n %1\n\n%2")
                .arg(path, task.error->message));
    };
    Automation::CommandContext context{
        .expected = runtime->documentVersion(),
        .source = Automation::InvocationSource::TrustedGui,
    };
    const auto accepted = runtime->extractions().startMidi(
        context, Automation::ClipId(audioClip->id()), std::move(observer));
    if (!accepted) {
        showExtractionError(MidiExtractController::tr("Failed to start MIDI extraction:\n\n%1")
                                .arg(accepted.getError().message));
    }
}
