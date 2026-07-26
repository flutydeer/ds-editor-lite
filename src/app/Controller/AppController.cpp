//
// Created by FlutyDeer on 2023/12/1.
//

#include "AppController.h"

#include "AppController_p.h"
#include "AudioDecodingController.h"
#include "ClipController.h"
#include "ProjectPackageResolver.h"
#include "ProjectStatusController.h"
#include "TrackController.h"
#include "Actions/AppModel/Tempo/TempoActions.h"
#include "Actions/AppModel/TimeSignature/TimeSignatureActions.h"
#include "Interface/IMainWindow.h"
#include <lite/ProjectModel/AppModel/Track.h>
#include "Model/AppOptions/AppOptions.h"
#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/ProjectConverters/MidiConverter.h"
#include <lite/Tasking/TaskManager.h>
#include "Tasks/DecodeAudioTask.h"
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Animation/AnimationGlobal.h>
#include <lite/Support/Log.h>

#include "Actions/AppModel/MasterControl/MasterControlActions.h"

#include <algorithm>

AppController::AppController(QObject *parent)
    : QObject(parent), d_ptr(new AppControllerPrivate(this)) {
    Q_D(AppController);
    AppControllerPrivate::initializeModules();
}

AppController::~AppController() {
    delete d_ptr;
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppController)

bool AppController::exportMidiFile(const QString &filePath) {
    MidiConverter converter;
    QString errMsg;
    Log::i("Midi exporter", errMsg);
    return converter.save(filePath, appModel, errMsg);
}

void AppController::onSetTempo(const double tempo) {
    const auto model = appModel;
    const auto oldTempo = model->timeline().tempoAt(0);
    const auto newTempo = tempo > 0 ? tempo : oldTempo;
    const auto actions = new TempoActions;
    actions->editTempo(oldTempo, newTempo, model);
    actions->execute();
    historyManager->record(actions);
}

void AppController::onSetTimeSignatureAt(const int barIndex, const int numerator,
                                         const int denominator) {
    const auto model = appModel;
    if (barIndex < 0 || numerator <= 0 || denominator <= 0 ||
        !AppControllerPrivate::isPowerOf2(denominator))
        return;

    // Skip when an existing point at this bar already has these values, so
    // live edits from the popup do not spam the undo stack with no-ops
    const auto &signatures = model->timeline().timeSignatures();
    const auto existing =
        std::find_if(signatures.cbegin(), signatures.cend(),
                     [barIndex](const TimeSignature &sig) { return sig.barIndex == barIndex; });
    if (existing != signatures.cend() && existing->numerator == numerator &&
        existing->denominator == denominator)
        return;

    const auto actions = new TimeSignatureActions;
    actions->setTimeSignatureAt(TimeSignature(barIndex, numerator, denominator), model);
    actions->execute();
    historyManager->record(actions);
}

void AppController::onRemoveTimeSignatureAt(const int barIndex) {
    const auto model = appModel;
    // The bar 0 anchor point is never removable
    if (barIndex <= 0)
        return;
    const auto &signatures = model->timeline().timeSignatures();
    const bool exists =
        std::any_of(signatures.cbegin(), signatures.cend(),
                    [barIndex](const TimeSignature &sig) { return sig.barIndex == barIndex; });
    if (!exists)
        return;

    const auto actions = new TimeSignatureActions;
    actions->removeTimeSignatureAt(barIndex, model);
    actions->execute();
    historyManager->record(actions);
}

void AppController::editMasterControl(const TrackControl &control) {
    const auto actions = new MasterControlActions;
    actions->editMasterControl(control, appModel);
    actions->execute();
    historyManager->record(actions);
}

void AppController::onUndoRedoChanged(const bool canUndo, const QString &undoActionName,
                                      const bool canRedo, const QString &redoActionName) {
    Q_D(AppController);
    Q_UNUSED(canUndo);
    Q_UNUSED(canRedo);
    Q_UNUSED(redoActionName);
    Q_UNUSED(undoActionName);
    d->m_mainWindow->updateWindowTitle();
}

void AppController::setMainWindow(IMainWindow *window) {
    Q_D(AppController);
    d->m_mainWindow = window;
}

void AppController::quit() {
    Q_D(AppController);
    d->m_mainWindow->quit();
}

void AppController::restart() {
    qDebug() << "restart";
    Q_D(AppController);
    d->m_mainWindow->restart();
}

void AppControllerPrivate::initializeModules() {
    InferEngine::instance();
    ProjectPackageResolver::instance();
    InferController::instance();
    ProjectStatusController::instance();

    // Read appearance settings and push them into the theme system, which no
    // longer depends on AppOptions.
    const auto pushAppearance = [] {
        const auto appearance = appOptions->appearance();
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(AnimationGlobal::fromString(appearance->animationLevel),
                                    appearance->animationTimeScale);
        theme->updateThemePreference(appearance->themeId);
    };
    pushAppearance();
    connect(appOptions, &AppOptions::optionsChanged, ThemeManager::instance(),
            [pushAppearance](AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::Appearance)
                    pushAppearance();
            });

    // Push app-provided new-track defaults into the model, which no longer
    // reaches into AppOptions / the app-wide color palette itself.
    appModel->setPaletteColorCount(AppGlobal::paletteColorCount);
    const auto pushModelDefaults = [] {
        appModel->setDefaultSingingLanguage(appOptions->general()->defaultSingingLanguage);
    };
    pushModelDefaults();
    connect(appOptions, &AppOptions::optionsChanged, appModel,
            [pushModelDefaults](AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::General)
                    pushModelDefaults();
            });

    connect(appModel, &AppModel::modelChanged, audioDecodingController,
            &AudioDecodingController::onModelChanged);
    connect(appModel, &AppModel::trackChanged, audioDecodingController,
            &AudioDecodingController::onTrackChanged);
}

bool AppControllerPrivate::isPowerOf2(const int num) {
    return num > 0 && (num & num - 1) == 0;
}
