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
    void addSubscriber(IAnimatable *object);
    void removeSubscriber(IAnimatable *object);

public slots:
    void onAppOptionsChanged();

private:
    QList<IAnimatable *> m_subscribers;
};



#endif // THEMEMANAGER_H
