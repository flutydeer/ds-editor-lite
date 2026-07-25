//
// Created by fluty on 24-3-15.
//

#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QColor>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

#include "UI/Utils/AnimationGlobal.h"

class IAnimatable;

class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class ThemeColorType { Light, Dark, HighContrast };

private:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;

public:
    static ThemeManager *instance();
    Q_DISABLE_COPY_MOVE(ThemeManager)

    // --- Theme loading ---
    bool initialize(const QString &themeId);
    bool applyTheme(const QString &themeId);
    bool applyThemePreference(const QString &themePreferenceId);
    bool reloadCurrentTheme();

    // --- Query ---
    QString currentThemeId() const;
    ThemeColorType colorType() const;
    QString styleSheet() const;
    QString lyricStyleSheet() const;
    [[nodiscard]] QColor semanticColor(const QString &token) const;

    // --- Style roots (windows that receive the full QSS) ---
    void addStyleRoot(QWidget *root);
    void removeStyleRoot(QWidget *root);

    // --- Frame effects (existing) ---
    void addAnimationObserver(IAnimatable *object);
    void removeAnimationObserver(IAnimatable *object);
    void addWindow(QWidget *window);
    void removeWindow(QWidget *window);

signals:
    void themeChanged(const QString &themeId);

public:
    // Theme preference and animation settings are pushed in by app code (which
    // reads AppOptions), so the theme system carries no settings dependency.
    void updateThemePreference(const QString &preferredThemeId);
    void setAnimationSettings(AnimationGlobal::AnimationLevels level, double timeScale);

    // The app owns its color palette (AppColorPalette); the theme system merely
    // loads palette colors from the theme file and hands them to this sink, so
    // the theme code carries no dependency on the app palette.
    void setPaletteSink(std::function<void(const QList<QColor> &)> sink);

public slots:
    void onSystemThemeColorChanged();

private:
    enum class ColorSchemePolicy { Explicit, FollowSystem };

    void applyAnimationSettings(IAnimatable *object) const;
    static QString normalizedThemePreferenceId(const QString &themePreferenceId);
    static QString systemThemeId();
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool applyThemeInternal(const QString &themeId, ColorSchemePolicy colorSchemePolicy);

    // --- Theme state ---
    QString m_currentThemeId;
    QString m_observedThemePreferenceId;
    bool m_followSystemTheme = false;
    ThemeColorType m_colorType = ThemeColorType::Dark;
    QString m_styleSheet;
    QString m_lyricStyleSheet;
    QHash<QString, QColor> m_semanticColors;

    // --- Animation ---
    QList<IAnimatable *> m_subscribers;
    AnimationGlobal::AnimationLevels m_animationLevel = AnimationGlobal::Full;
    double m_animationTimeScale = 1;

    // --- Palette (owned by the app; pushed out via the sink) ---
    QList<QColor> m_paletteColors;
    std::function<void(const QList<QColor> &)> m_paletteSink;

    // --- Frame windows ---
    QList<QPointer<QWidget>> m_windows;

    // --- Style roots ---
    QList<QPointer<QWidget>> m_styleRoots;
};

#endif // THEMEMANAGER_H
