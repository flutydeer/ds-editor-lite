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
#include <QGuiApplication>
#include <QPointer>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleHints>
#include <QTimer>
#include <QWidget>

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { onSystemThemeColorChanged(); });
#endif
}

ThemeManager::~ThemeManager() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(ThemeManager)

// ── Theme loading ────────────────────────────────────────────────────────

bool ThemeManager::initialize(const QString &themeId) {
    m_observedThemePreferenceId = normalizedThemePreferenceId(appOptions->appearance()->themeId);
    return applyThemePreference(themeId);
}

bool ThemeManager::applyTheme(const QString &themeId) {
    if (!applyThemeInternal(themeId, ColorSchemePolicy::Explicit))
        return false;

    m_followSystemTheme = false;
    return true;
}

bool ThemeManager::applyThemePreference(const QString &themePreferenceId) {
    const auto normalizedThemeId = normalizedThemePreferenceId(themePreferenceId);
    const auto shouldFollowSystem =
        normalizedThemeId == AppearanceOption::systemThemePreferenceId();
    const auto themeId = shouldFollowSystem
                             ? systemThemeId()
                             : AppearanceOption::themeIdForPreference(normalizedThemeId);
    if (!applyThemeInternal(themeId, shouldFollowSystem ? ColorSchemePolicy::FollowSystem
                                                        : ColorSchemePolicy::Explicit)) {
        return false;
    }

    m_followSystemTheme = shouldFollowSystem;
    return true;
}

bool ThemeManager::applyThemeInternal(const QString &themeId,
                                      const ColorSchemePolicy colorSchemePolicy) {
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    const QSignalBlocker blocker(QGuiApplication::styleHints());
    if (colorSchemePolicy == ColorSchemePolicy::FollowSystem) {
        QGuiApplication::styleHints()->unsetColorScheme();
    } else {
        QGuiApplication::styleHints()->setColorScheme(
            m_colorType == ThemeColorType::Light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
    }
#elif QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const QSignalBlocker blocker(QGuiApplication::styleHints());
    QGuiApplication::styleHints()->setColorScheme(
        colorSchemePolicy == ColorSchemePolicy::FollowSystem
            ? Qt::ColorScheme::Unknown
            : (m_colorType == ThemeColorType::Light ? Qt::ColorScheme::Light
                                                    : Qt::ColorScheme::Dark));
#endif

    // Apply QSS to all registered style roots
    for (const auto &ptr : std::as_const(m_styleRoots)) {
        if (auto *root = ptr.data())
            root->setStyleSheet(m_styleSheet);
    }

    for (const auto &windowPtr : std::as_const(m_windows)) {
        if (auto *window = windowPtr.data())
            WindowFrameUtils::applyFrameEffects(window);
    }

    emit themeChanged(m_currentThemeId);
    return true;
}

bool ThemeManager::reloadCurrentTheme() {
    if (m_currentThemeId.isEmpty())
        return false;
    if (m_followSystemTheme)
        return applyThemePreference(AppearanceOption::systemThemePreferenceId());
    return applyThemeInternal(m_currentThemeId, ColorSchemePolicy::Explicit);
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

    const auto preferredThemeId = normalizedThemePreferenceId(appOptions->appearance()->themeId);
    if (preferredThemeId == m_observedThemePreferenceId)
        return;

    if ((preferredThemeId == AppearanceOption::systemThemePreferenceId() && m_followSystemTheme) ||
        (preferredThemeId != AppearanceOption::systemThemePreferenceId() &&
         AppearanceOption::themeIdForPreference(preferredThemeId) == m_currentThemeId)) {
        m_observedThemePreferenceId = preferredThemeId;
        return;
    }

    if (applyThemePreference(preferredThemeId)) {
        m_observedThemePreferenceId = preferredThemeId;
    } else {
        qWarning().noquote() << "Failed to apply preferred theme" << preferredThemeId << ":"
                             << ThemeLoader::lastError();
    }
}

void ThemeManager::onSystemThemeColorChanged() {
    if (m_followSystemTheme) {
        const auto themeId = systemThemeId();
        if (themeId != m_currentThemeId) {
            if (!applyThemeInternal(themeId, ColorSchemePolicy::FollowSystem)) {
                qWarning().noquote()
                    << "Failed to apply system theme" << themeId << ":" << ThemeLoader::lastError();
            }
            return;
        }
    }

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

QString ThemeManager::normalizedThemePreferenceId(const QString &themePreferenceId) {
    const auto normalizedThemeId = themePreferenceId.trimmed();
    if (normalizedThemeId.isEmpty())
        return AppearanceOption::systemThemePreferenceId();
    return normalizedThemeId;
}

QString ThemeManager::systemThemeId() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    const QSignalBlocker blocker(QGuiApplication::styleHints());
    QGuiApplication::styleHints()->unsetColorScheme();
#elif QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const QSignalBlocker blocker(QGuiApplication::styleHints());
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto colorScheme = QGuiApplication::styleHints()->colorScheme();
    if (colorScheme == Qt::ColorScheme::Light)
        return AppearanceOption::lightThemeId();
#endif
    return AppearanceOption::defaultThemeId();
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
