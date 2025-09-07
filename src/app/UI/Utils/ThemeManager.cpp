//
// Created by fluty on 24-3-15.
//

#include "ThemeManager.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/IAnimatable.h"
#include "Utils/WindowFrameUtils.h"

#include <QEvent>
#include <QPointer>
#include <QStyle>
#include <QTimer>
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
    if (event->type() == QEvent::Show) {
        // If the shown object is one of our managed windows, defer the polish.
        for (const auto window : std::as_const(m_windows)) {
            if (window == watched) {
                // Use QPointer to avoid operating on a deleted widget if it is destroyed
                // before the singleShot lambda runs.
                QPointer<QWidget> wp(window);

                QTimer::singleShot(0, this, [wp]() {
                    if (!wp) {
                        // widget was deleted
                        return;
                    }

                    WindowFrameUtils::applyFrameEffects(wp);
                    // if (SystemUtils::isWindows()) {
                    //     if (QSysInfo::productVersion() == "11")
                    //         wp->setProperty("transparentWindow", true);
                    //     else
                    //         wp->setProperty("transparentWindow", false);
                    // } else
                    //     wp->setProperty("transparentWindow", false);
                    wp->setProperty("transparentWindow", false);

                    // Only re-polish after the current event loop iteration to avoid
                    // re-entrant style events on styles like Breeze.
                    if (wp->style()) {
                        wp->style()->unpolish(wp);
                        wp->style()->polish(wp);
                        // Force a polish-driven update
                        wp->update();
                    }
                });

                // No need to continue loop after match
                break;
            }
        }
    }

    // Let normal processing continue
    return QObject::eventFilter(watched, event);
}
