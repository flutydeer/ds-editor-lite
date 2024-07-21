//
// Created by fluty on 24-3-13.
//

#ifndef APPOPTIONS_H
#define APPOPTIONS_H

#define appOptions AppOptions::instance()

#include <QObject>

#include "Utils/Singleton.h"
#include "Options/AppearanceOption.h"
#include "Options/AudioOption.h"
#include "Options/LanguageOption.h"
#include "Options/FillLyricOption.h"

class AppOptions : public QObject, public Singleton<AppOptions> {
    Q_OBJECT

public:
    explicit AppOptions(QObject *parent = nullptr);

    // void load(const QJsonObject &object);
    // void save(const QString &path);
    bool saveAndNotify();

    AudioOption *audio();
    AppearanceOption *appearance();
    LanguageOption *language();
    FillLyricOption *fillLyric();

signals:
    void optionsChanged();

private:
    AudioOption m_audioOption;
    AppearanceOption m_appearanceOption;
    LanguageOption m_languageOption;
    FillLyricOption m_fillLyricOption;

    QString fileName = "appConfig.json";
    QString m_configPath;

    void notifyOptionsChanged();
};

#endif // APPOPTIONS_H
