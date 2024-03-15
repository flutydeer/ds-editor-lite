//
// Created by fluty on 24-3-15.
//

#include "ThemeManager.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/IAnimatable.h"

void ThemeManager::addSubscriber(IAnimatable *object) {
    m_subscribers += object;
    qDebug() << "ThemeManager::addSubscriber" << object;
}
void ThemeManager::removeSubscriber(IAnimatable *object) {
    m_subscribers.removeOne(object);
}
void ThemeManager::onAppOptionsChanged() {
    qDebug() << "ThemeManager::onAppOptionsChanged";
    auto option = AppOptions::instance()->appearance();
    auto level = option->animationLevel;
    auto scale = option->animationTimeScale;
    qDebug() << level << scale;
    for (auto object : m_subscribers) {
        object->setAnimationLevel(level);
        object->setTimeScale(scale);
    }
}