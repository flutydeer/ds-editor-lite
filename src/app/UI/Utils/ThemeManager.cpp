//
// Created by fluty on 24-3-15.
//

#include "ThemeManager.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/IAnimatable.h"

void ThemeManager::addAnimationObserver(IAnimatable *object) {
    m_subscribers += object;
    applyAnimationSetings(object);
    // qDebug() << "ThemeManager::addAnimationObserver" << object;
}
void ThemeManager::removeAnimationObserver(IAnimatable *object) {
    m_subscribers.removeOne(object);
}
void ThemeManager::onAppOptionsChanged() {
    // qDebug() << "ThemeManager::onAppOptionsChanged";
    for (auto object : m_subscribers)
        applyAnimationSetings(object);
}
void ThemeManager::applyAnimationSetings(IAnimatable *object) {
    auto option = AppOptions::instance()->appearance();
    auto level = option->animationLevel;
    auto scale = option->animationTimeScale;
    object->setAnimationLevel(level);
    object->setTimeScale(scale);
}