//
// Created by fluty on 24-3-13.
//

#ifndef APPOPTIONS_H
#define APPOPTIONS_H

#define appOptions AppOptions::instance()

#include "Utils/Singleton.h"
#include "Options/AppearanceOption.h"
#include "Options/AudioOption.h"
#include "Options/LanguageOption.h"
#include "Options/FillLyricOption.h"
#include "Options/GeneralOption.h"
#include "Options/InferenceOption.h"
#include "Global/AppOptionsGlobal.h"

class AppOptions : public QObject, public Singleton<AppOptions> {
    Q_OBJECT

public:
    explicit AppOptions(QObject *parent = nullptr);

    [[nodiscard]] QString configPath() const;

    // void load(const QJsonObject &object);
    // void save(const QString &path);
    bool saveAndNotify(AppOptionsGlobal::Option option);

    GeneralOption *general();
    AudioOption *audio();
    AppearanceOption *appearance();
    LanguageOption *language();
    FillLyricOption *fillLyric();
    InferenceOption *inference();

signals:
    void optionsChanged(AppOptionsGlobal::Option option);

private:
    GeneralOption m_generalOption;
    AudioOption m_audioOption;
    AppearanceOption m_appearanceOption;
    LanguageOption m_languageOption;
    FillLyricOption m_fillLyricOption;
    InferenceOption m_inferenceOption;

    QString fileName = "appConfig.json";
    QString m_configPath;

    void notifyOptionsChanged(AppOptionsGlobal::Option option);
};

#endif // APPOPTIONS_H
