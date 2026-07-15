//
// Created by fluty on 24-3-13.
//

#ifndef APPOPTIONS_H
#define APPOPTIONS_H

#define appOptions AppOptions::instance()

#include "Utils/Singleton.h"
#include "Options/AppearanceOption.h"
#include "Options/AudioOption.h"
#include "Options/DeveloperOption.h"
#include "Options/G2pLanguageOption.h"
#include "Options/FillLyricOption.h"
#include "Options/GeneralOption.h"
#include "Options/InferenceOption.h"
#include "Global/AppOptionsGlobal.h"

class AppOptions : public QObject {
    Q_OBJECT

public:
    explicit AppOptions(QObject *parent = nullptr);
    ~AppOptions() override;

    LITE_SINGLETON_DECLARE_INSTANCE(AppOptions)
    Q_DISABLE_COPY_MOVE(AppOptions)

public:
    [[nodiscard]] QString configPath() const;

    bool saveAndNotify(AppOptionsGlobal::Option option);

    GeneralOption *general();
    AudioOption *audio();
    AppearanceOption *appearance();
    G2pLanguageOption *g2pLanguage();
    FillLyricOption *fillLyric();
    InferenceOption *inference();
    DeveloperOption *developer();

signals:
    void optionsChanged(AppOptionsGlobal::Option option);

private:
    GeneralOption m_generalOption;
    AudioOption m_audioOption;
    AppearanceOption m_appearanceOption;
    G2pLanguageOption m_g2pLanguageOption;
    FillLyricOption m_fillLyricOption;
    InferenceOption m_inferenceOption;
    DeveloperOption m_developerOption;

    QString fileName = "appConfig.json";
    QString m_configPath;

    void notifyOptionsChanged(AppOptionsGlobal::Option option);
};

#endif // APPOPTIONS_H
