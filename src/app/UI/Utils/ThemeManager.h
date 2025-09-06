//
// Created by fluty on 24-3-15.
//

#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include "Utils/Singleton.h"
#include "Global/AppOptionsGlobal.h"

class IAnimatable;

class ThemeManager : public QObject {
public:
    enum class ThemeColorType { Light, Dark, HighContrast };

private:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(ThemeManager)
    Q_DISABLE_COPY_MOVE(ThemeManager)

public:
    void addAnimationObserver(IAnimatable *object);
    void removeAnimationObserver(IAnimatable *object);
    void addWindow(QWidget *window);
    void removeWindow(QWidget *window);

public slots:
    void onAppOptionsChanged(AppOptionsGlobal::Option option);
    void onSystemThemeColorChanged();

private:
    static void applyAnimationSettings(IAnimatable *object);
    bool eventFilter(QObject *watched, QEvent *event) override;

    QList<IAnimatable *> m_subscribers;
    QList<QWidget *> m_windows;
};

#endif // THEMEMANAGER_H
