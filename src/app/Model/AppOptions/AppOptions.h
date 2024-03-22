//
// Created by fluty on 24-3-13.
//

#ifndef APPOPTIONS_H
#define APPOPTIONS_H

#include <QObject>

#include "Utils/Singleton.h"
#include "Options/AppearanceOption.h"
#include "Options/AudioOption.h"

class AppOptions : public QObject, public Singleton<AppOptions> {
    Q_OBJECT

public:
    explicit AppOptions(QObject *parent = nullptr);

    // void load(const QJsonObject &object);
    // void save(const QString &path);
    bool saveAndNotify();

    AudioOption *audio();
    AppearanceOption *appearance();

signals:
    void optionsChanged();

private:
    AudioOption m_audioOption;
    AppearanceOption m_appearanceOption;

    QString fileName = "appConfig.json";
    QString m_configPath;

    void notifyOptionsChanged();
};

#endif // APPOPTIONS_H
