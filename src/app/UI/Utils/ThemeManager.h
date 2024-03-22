//
// Created by fluty on 24-3-15.
//

#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include "Utils/Singleton.h"

class IAnimatable;

class ThemeManager : public QObject, public Singleton<ThemeManager> {
public:
    void addAnimationObserver(IAnimatable *object);
    void removeAnimationObserver(IAnimatable *object);

public slots:
    void onAppOptionsChanged();

private:
    QList<IAnimatable *> m_subscribers;
    static void applyAnimationSetings(IAnimatable *object);
};



#endif // THEMEMANAGER_H
