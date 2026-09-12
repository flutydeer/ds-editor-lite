#include "AppController.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppOptions/AppOptions.h"
#include "Utils/FontManager.h"
#include <lite/GUI/Theme/ThemeManager.h>

AppController::AppController(QObject *parent) : QObject(parent) {
    // Read appearance settings and push them into the theme system, which no
    // longer depends on AppOptions.
    const auto pushAppearance = [] {
        const auto appearance = appOptions->appearance();
        auto *theme = ThemeManager::instance();
        theme->setAnimationSettings(appearance->animationEnabled, appearance->animationTimeScale);
        theme->updateThemePreference(appearance->themeId);
        FontManager::instance().applyInterfaceFont(appearance->uiFontFamily);
    };
    pushAppearance();
    connect(appOptions, &AppOptions::optionsChanged, ThemeManager::instance(),
            [pushAppearance](AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::Appearance)
                    pushAppearance();
            });
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppController)

bool AppController::exportMidiFile(const QString &filePath) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return false;
    const auto result =
        runtime->files().exportMidi({.expected = runtime->documentVersion(),
                                     .source = Automation::InvocationSource::TrustedGui},
                                    filePath, true);
    return static_cast<bool>(result);
}

void AppController::onSetTempo(const double tempo) {
    appController->onSetTempoAt(0, tempo);
}

void AppController::onSetTempoAt(const int tick, const double tempo) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    Automation::CommandContext context{.expected = runtime->documentVersion()};
    runtime->timeline().setTempo(context, tick, tempo);
}

void AppController::onRemoveTempoAt(const int tick) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    Automation::CommandContext context{.expected = runtime->documentVersion()};
    runtime->timeline().deleteTempo(context, tick);
}

void AppController::onSetTimeSignatureAt(const int barIndex, const int numerator,
                                         const int denominator) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    Automation::CommandContext context{.expected = runtime->documentVersion()};
    runtime->timeline().setTimeSignature(context, barIndex, numerator, denominator);
}

void AppController::onRemoveTimeSignatureAt(const int barIndex) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    Automation::CommandContext context{.expected = runtime->documentVersion()};
    runtime->timeline().deleteTimeSignature(context, barIndex);
}

void AppController::editMasterControl(const TrackControl &control) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    Automation::CommandContext context{.expected = runtime->documentVersion()};
    runtime->timeline().setMasterControl(context, control);
}

void AppController::quit() {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    runtime->application().requestTermination({.source = Automation::InvocationSource::TrustedGui},
                                              Automation::ApplicationTerminationMode::Exit);
}

void AppController::restart() {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    runtime->application().requestTermination({.source = Automation::InvocationSource::TrustedGui},
                                              Automation::ApplicationTerminationMode::Restart);
}
