#include "MidiExtractController.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "UI/Dialogs/Base/Dialog.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

namespace {
    Automation::CoreRuntime *automationRuntime() {
        return AppContext::instance<Automation::CoreRuntime>();
    }

    void showExtractionError(const QString &message) {
        Dialog dialog;
        dialog.setTitle(MidiExtractController::tr("Task Failed"));
        dialog.setMessage(message);
        dialog.setModal(true);
        const auto close = new AccentButton(MidiExtractController::tr("Close"));
        QObject::connect(close, &Button::clicked, &dialog, &Dialog::accept);
        dialog.setPositiveButton(close);
        dialog.exec();
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
