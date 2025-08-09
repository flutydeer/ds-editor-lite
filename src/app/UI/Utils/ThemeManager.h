//
// Created by fluty on 24-3-15.
//

#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include "Utils/Singleton.h"
#include "Global/AppOptionsGlobal.h"

class IAnimatable;

class ThemeManager : public QObject, public Singleton<ThemeManager> {
public:
    enum class ThemeColorType { Light, Dark, HighContrast };
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
