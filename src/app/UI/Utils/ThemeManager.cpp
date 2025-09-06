//
// Created by fluty on 24-3-15.
//

#include "ThemeManager.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/IAnimatable.h"
#include "Utils/WindowFrameUtils.h"

#include <QEvent>
#include <QStyle>
#include <QWidget>

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
}

ThemeManager::~ThemeManager() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(ThemeManager)

void ThemeManager::addAnimationObserver(IAnimatable *object) {
    m_subscribers += object;
    applyAnimationSettings(object);
    // qDebug() << "ThemeManager::addAnimationObserver" << object;
}

void ThemeManager::removeAnimationObserver(IAnimatable *object) {
    m_subscribers.removeOne(object);
}

void ThemeManager::addWindow(QWidget *window) {
    m_windows += window;
    window->installEventFilter(this);
}

void ThemeManager::removeWindow(QWidget *window) {
    m_windows.removeOne(window);
    window->removeEventFilter(this);
}

void ThemeManager::onAppOptionsChanged(const AppOptionsGlobal::Option option) {
    // qDebug() << "ThemeManager::onAppOptionsChanged";
    if (option != AppOptionsGlobal::All && option != AppOptionsGlobal::Appearance)
        return;

    for (const auto object : m_subscribers)
        applyAnimationSettings(object);
}

void ThemeManager::onSystemThemeColorChanged() {
    for (const auto window : m_windows)
        WindowFrameUtils::applyFrameEffects(window);
}

void ThemeManager::applyAnimationSettings(IAnimatable *object) {
    const auto option = appOptions->appearance();
    const auto level = option->animationLevel;
    const auto scale = option->animationTimeScale;
    object->setAnimationLevel(level);
    object->setTimeScale(scale);
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Show)
        for (const auto window : m_windows)
            if (window == watched) {
                WindowFrameUtils::applyFrameEffects(window);
                // if (SystemUtils::isWindows()) {
                //     if (QSysInfo::productVersion() == "11")
                //         window->setProperty("transparentWindow", true);
                //     else
                //         window->setProperty("transparentWindow", false);
                // } else
                //     window->setProperty("transparentWindow", false);
                window->setProperty("transparentWindow", false);
                window->style()->unpolish(window);
                window->style()->polish(window);
            }

    return QObject::eventFilter(watched, event);
}
