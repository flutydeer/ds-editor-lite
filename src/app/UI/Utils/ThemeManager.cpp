//
// Created by fluty on 24-3-15.
//

#include "ThemeManager.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/IAnimatable.h"
#include "Utils/WindowFrameUtils.h"

#include <QEvent>
#include <QWidget>

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

void ThemeManager::onAppOptionsChanged() {
    // qDebug() << "ThemeManager::onAppOptionsChanged";
    for (auto object : m_subscribers)
        applyAnimationSettings(object);
}

void ThemeManager::onSystemThemeColorChanged(ThemeColorType colorType) {
    for (auto window : m_windows)
        WindowFrameUtils::applyFrameEffects(window);
}

void ThemeManager::applyAnimationSettings(IAnimatable *object) {
    auto option = appOptions->appearance();
    auto level = option->animationLevel;
    auto scale = option->animationTimeScale;
    object->setAnimationLevel(level);
    object->setTimeScale(scale);
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Show)
        for (auto window : m_windows)
            if (window == watched)
                WindowFrameUtils::applyFrameEffects(window);

    return QObject::eventFilter(watched, event);
}
