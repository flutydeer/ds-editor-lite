//
// Created by fluty on 24-3-15.
//

#include "ThemeManager.h"

#include "ThemeIds.h"
#include "UI/Utils/IAnimatable.h"
#include "Theme/ThemeLoader.h"
#include "UI/Utils/WindowFrameUtils.h"

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

ThemeManager *ThemeManager::instance() {
    static ThemeManager obj;
    return &obj;
}

void ThemeManager::setPaletteSink(std::function<void(const QList<QColor> &)> sink) {
    m_paletteSink = std::move(sink);
    // Push the current palette immediately so a sink registered after the initial
    // theme has been applied still receives it.
    if (m_paletteSink && !m_paletteColors.isEmpty())
        m_paletteSink(m_paletteColors);
}

// ── Theme loading ────────────────────────────────────────────────────────

bool ThemeManager::initialize(const QString &themeId) {
    m_observedThemePreferenceId = normalizedThemePreferenceId(themeId);
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
        normalizedThemeId == ThemeIds::systemThemePreferenceId();
    const auto themeId = shouldFollowSystem
                             ? systemThemeId()
                             : ThemeIds::themeIdForPreference(normalizedThemeId);
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

    // Hand the loaded palette to the app-owned palette via the sink.
    m_paletteColors = def->paletteColors;
    if (m_paletteSink && !m_paletteColors.isEmpty())
        m_paletteSink(m_paletteColors);

    // Store state
    m_currentThemeId = def->folderName;
    m_styleSheet = def->styleSheet;
    m_lyricStyleSheet = def->lyricStyleSheet;
    m_semanticColors = def->semanticColors;

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
        return applyThemePreference(ThemeIds::systemThemePreferenceId());
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

QColor ThemeManager::semanticColor(const QString &token) const {
    return m_semanticColors.value(token);
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

void ThemeManager::updateThemePreference(const QString &preferredThemeId) {
    const auto normalized = normalizedThemePreferenceId(preferredThemeId);
    if (normalized == m_observedThemePreferenceId)
        return;

    if ((normalized == ThemeIds::systemThemePreferenceId() && m_followSystemTheme) ||
        (normalized != ThemeIds::systemThemePreferenceId() &&
         ThemeIds::themeIdForPreference(normalized) == m_currentThemeId)) {
        m_observedThemePreferenceId = normalized;
        return;
    }

    if (applyThemePreference(normalized)) {
        m_observedThemePreferenceId = normalized;
    } else {
        qWarning().noquote() << "Failed to apply preferred theme" << normalized << ":"
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

void ThemeManager::applyAnimationSettings(IAnimatable *object) const {
    object->setAnimationLevel(m_animationLevel);
    object->setTimeScale(m_animationTimeScale);
}

void ThemeManager::setAnimationSettings(AnimationGlobal::AnimationLevels level, double timeScale) {
    m_animationLevel = level;
    m_animationTimeScale = timeScale;
    for (auto *object : m_subscribers)
        applyAnimationSettings(object);
}

QString ThemeManager::normalizedThemePreferenceId(const QString &themePreferenceId) {
    const auto normalizedThemeId = themePreferenceId.trimmed();
    if (normalizedThemeId.isEmpty())
        return ThemeIds::systemThemePreferenceId();
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
        return ThemeIds::lightThemeId();
#endif
    return ThemeIds::defaultThemeId();
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
