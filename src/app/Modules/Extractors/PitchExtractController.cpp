#include "PitchExtractController.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "UI/Dialogs/Base/Dialog.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

namespace {
    Automation::CoreRuntime *automationRuntime() {
        return AppContext::instance<Automation::CoreRuntime>();
    }

    void showExtractionError(const QString &message) {
        Dialog dialog;
        dialog.setTitle(PitchExtractController::tr("Task Failed"));
        dialog.setMessage(message);
        dialog.setModal(true);
        const auto close = new AccentButton(PitchExtractController::tr("Close"));
        QObject::connect(close, &Button::clicked, &dialog, &Dialog::accept);
        dialog.setPositiveButton(close);
        dialog.exec();
    }
}

PitchExtractController::PitchExtractController(QObject *parent) : ModelChangeHandler(parent) {
}

PitchExtractController::~PitchExtractController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(PitchExtractController)

void PitchExtractController::runExtractPitch(const AudioClip *audioClip,
                                             const SingingClip *singingClip) {
    auto *runtime = automationRuntime();
    if (!runtime || !audioClip || !singingClip)
        return;

    const auto path = audioClip->path();
    Automation::ExtractionObserver observer;
    observer.finished = [path](const Automation::AutomationTaskSnapshot &task) {
        if (task.state != Automation::AutomationTaskState::Failed || !task.error)
            return;
        showExtractionError(
            PitchExtractController::tr("Failed to extract pitch from audio:\n %1\n\n%2")
                .arg(path, task.error->message));
    };
    Automation::CommandContext context{
        .expected = runtime->documentVersion(),
        .source = Automation::InvocationSource::TrustedGui,
    };
    const auto accepted = runtime->extractions().startPitch(
        context, Automation::ClipId(audioClip->id()), Automation::ClipId(singingClip->id()),
        std::move(observer));
    if (!accepted) {
        showExtractionError(PitchExtractController::tr("Failed to start pitch extraction:\n\n%1")
                                .arg(accepted.getError().message));
    }
}
