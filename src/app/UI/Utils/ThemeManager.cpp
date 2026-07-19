//
// Created by fluty on 24-3-15.
//

#include "ThemeManager.h"

#include "AppColorPalette.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Utils/IAnimatable.h"
#include "Theme/ThemeLoader.h"
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

// ── Theme loading ────────────────────────────────────────────────────────

bool ThemeManager::initialize(const QString &themeId) {
    return applyTheme(themeId);
}

bool ThemeManager::applyTheme(const QString &themeId) {
    auto def = ThemeLoader::load(themeId);
    if (!def)
        return false;

    // Apply palette
    if (!def->paletteColors.isEmpty())
        AppColorPalette::instance()->setColors(def->paletteColors);

    // Store state
    m_currentThemeId = def->folderName;
    m_styleSheet = def->styleSheet;
    m_lyricStyleSheet = def->lyricStyleSheet;

    // Map color type string to enum
    if (def->colorType == QStringLiteral("light"))
        m_colorType = ThemeColorType::Light;
    else if (def->colorType == QStringLiteral("highContrast"))
        m_colorType = ThemeColorType::HighContrast;
    else
        m_colorType = ThemeColorType::Dark;

    // Apply QSS to all registered style roots
    for (const auto &ptr : std::as_const(m_styleRoots)) {
        if (auto *root = ptr.data())
            root->setStyleSheet(m_styleSheet);
    }

    emit themeChanged(themeId);
    return true;
}

bool ThemeManager::reloadCurrentTheme() {
    if (m_currentThemeId.isEmpty())
        return false;
    return applyTheme(m_currentThemeId);
}

// ── Query ────────────────────────────────────────────────────────────────

QString ThemeManager::currentThemeId() const {
    return m_currentThemeId;
}

ThemeManager::ThemeColorType ThemeManager::colorType() const {
    return m_colorType;
}

QString ThemeManager::styleSheet() const {
    return m_styleSheet;
}

QString ThemeManager::lyricStyleSheet() const {
    return m_lyricStyleSheet;
}

// ── Style roots ──────────────────────────────────────────────────────────

void ThemeManager::addStyleRoot(QWidget *root) {
    if (!root)
        return;

    QPointer<QWidget> ptr(root);
    if (m_styleRoots.contains(ptr))
        return;

    m_styleRoots.append(ptr);

    // Immediately apply the current stylesheet
    if (!m_styleSheet.isEmpty())
        root->setStyleSheet(m_styleSheet);

    // Auto-remove when the widget is destroyed
    connect(root, &QObject::destroyed, this, [this, ptr]() { m_styleRoots.removeAll(ptr); });
}

void ThemeManager::removeStyleRoot(QWidget *root) {
    if (!root)
        return;

    root->setStyleSheet(QString());
    QPointer<QWidget> ptr(root);
    m_styleRoots.removeAll(ptr);
}

// ── Animation ────────────────────────────────────────────────────────────

void ThemeManager::addAnimationObserver(IAnimatable *object) {
    m_subscribers += object;
    applyAnimationSettings(object);
}

void ThemeManager::removeAnimationObserver(IAnimatable *object) {
    m_subscribers.removeOne(object);
}

// ── Frame windows ────────────────────────────────────────────────────────

void ThemeManager::addWindow(QWidget *window) {
    if (!window)
        return;

    QPointer<QWidget> ptr(window);
    m_windows.append(ptr);
    window->installEventFilter(this);

    // Auto-remove on destroy
    connect(window, &QObject::destroyed, this, [this, ptr]() { m_windows.removeAll(ptr); });
}

void ThemeManager::removeWindow(QWidget *window) {
    if (!window)
        return;

    QPointer<QWidget> ptr(window);
    m_windows.removeAll(ptr);
    window->removeEventFilter(this);
}

// ── Slots ────────────────────────────────────────────────────────────────

void ThemeManager::onAppOptionsChanged(const AppOptionsGlobal::Option option) {
    if (option != AppOptionsGlobal::All && option != AppOptionsGlobal::Appearance)
        return;

    for (const auto object : m_subscribers)
        applyAnimationSettings(object);
}

void ThemeManager::onSystemThemeColorChanged() {
    for (const auto &windowPtr : std::as_const(m_windows)) {
        if (auto *window = windowPtr.data())
            WindowFrameUtils::applyFrameEffects(window);
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────

void ThemeManager::applyAnimationSettings(IAnimatable *object) {
    const auto option = appOptions->appearance();
    const auto level = option->animationLevel;
    const auto scale = option->animationTimeScale;
    object->setAnimationLevel(level);
    object->setTimeScale(scale);
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Show) {
        for (const auto &windowPtr : std::as_const(m_windows)) {
            if (windowPtr.data() == watched) {
                QPointer<QWidget> wp(windowPtr.data());

                QTimer::singleShot(0, this, [wp]() {
                    if (!wp)
                        return;

                    WindowFrameUtils::applyFrameEffects(wp);
                    wp->setProperty("transparentWindow", false);

                    if (wp->style()) {
                        wp->style()->unpolish(wp);
                        wp->style()->polish(wp);
                        wp->update();
                    }
                });

                break;
            }
        }
    }

    return QObject::eventFilter(watched, event);
}
