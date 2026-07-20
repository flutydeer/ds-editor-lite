//
// Created by fluty on 24-3-15.
//

#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QPointer>
#include <QString>

#include "Utils/Singleton.h"
#include "Global/AppOptionsGlobal.h"

class IAnimatable;

class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class ThemeColorType { Light, Dark, HighContrast };

private:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(ThemeManager)
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

public slots:
    void onAppOptionsChanged(AppOptionsGlobal::Option option);
    void onSystemThemeColorChanged();

private:
    enum class ColorSchemePolicy { Explicit, FollowSystem };

    static void applyAnimationSettings(IAnimatable *object);
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

    // --- Animation ---
    QList<IAnimatable *> m_subscribers;

    // --- Frame windows ---
    QList<QPointer<QWidget>> m_windows;

    // --- Style roots ---
    QList<QPointer<QWidget>> m_styleRoots;
};

#endif // THEMEMANAGER_H
