//
// Created by fluty on 24-3-13.
//

#ifndef APPOPTIONS_H
#define APPOPTIONS_H

#include <QObject>

#include "Utils/Singleton.h"
#include "Options/AppearanceOption.h"

class AppOptions : public QObject, public Singleton<AppOptions> {
    Q_OBJECT

public:
    explicit AppOptions(QObject *parent = nullptr);

    // void load(const QJsonObject &object);
    // void save(const QString &path);
    bool save();
    void notifyOptionsChanged();

    AppearanceOption *appearance();

signals:
    void optionsChanged();

private:
    AppearanceOption m_appearanceOption;
    QString fileName = "appConfig.json";
    QString m_configPath;

};

#endif // APPOPTIONS_H
