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
#include "Options/GeneralOption.h"
#include "Options/InferenceOption.h"

class AppOptions : public QObject, public Singleton<AppOptions> {
    Q_OBJECT

public:
    explicit AppOptions(QObject *parent = nullptr);

    [[nodiscard]] QString configPath() const;

    // void load(const QJsonObject &object);
    // void save(const QString &path);
    bool saveAndNotify();

    GeneralOption *general();
    AudioOption *audio();
    AppearanceOption *appearance();
    LanguageOption *language();
    FillLyricOption *fillLyric();
    InferenceOption *inference();

signals:
    void optionsChanged();

private:
    GeneralOption m_generalOption;
    AudioOption m_audioOption;
    AppearanceOption m_appearanceOption;
    LanguageOption m_languageOption;
    FillLyricOption m_fillLyricOption;
    InferenceOption m_inferenceOption;

    QString fileName = "appConfig.json";
    QString m_configPath;

    void notifyOptionsChanged();
};

#endif // APPOPTIONS_H
