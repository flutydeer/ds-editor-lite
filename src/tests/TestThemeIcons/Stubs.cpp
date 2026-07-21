#include "AppContext.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/ThemeManager.h"
#include "Utils/WindowFrameUtils.h"

template <> ThemeManager *AppContext::instance<ThemeManager>() {
    return nullptr;
}

template <> AppColorPalette *AppContext::instance<AppColorPalette>() {
    return nullptr;
}

AppOptions *AppOptions::instance() {
    return nullptr;
}

AppearanceOption *AppOptions::appearance() {
    return nullptr;
}

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
void WindowFrameUtils::applyFrameEffects(QWidget *) {
}
#endif
