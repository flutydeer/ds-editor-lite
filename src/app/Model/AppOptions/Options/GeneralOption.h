//
// Created by fluty on 24-8-2.
//

#ifndef GENERALOPTION_H
#define GENERALOPTION_H

// #include <QVersionNumber>

#include <QMap>

#include "Global/AppGlobal.h"
#include "Model/AppModel/Params.h"
#include "Model/AppOptions/IOption.h"
#include "Utils/Property.h"

class GeneralOption final : public IOption {
public:
    explicit GeneralOption() : IOption("general") {};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;
    void setPackageSearchPathsAndNotify(QStringList paths);
    QString defaultLyricForLanguage(const QString &language) const;

    QString defaultSingingLanguage = "cmn";
    QMap<QString, QString> defaultLyrics{
        {"cmn", "啦"},
        {"eng", "la"},
        {"jpn", "ら"},
        {"yue", "啦"}
    };
    QStringList packageSearchPaths;
#if false
    QVersionNumber defaultPackageVersion;
    LITE_OPTION_ITEM(QString, defaultPackage, QString())
    LITE_OPTION_ITEM(QString, defaultPackageId, QString())
    LITE_OPTION_ITEM(QString, defaultSingerId, QString())
    LITE_OPTION_ITEM(QString, defaultSpeakerId, QString())
#endif
    LITE_OPTION_ITEM(QString, gameDir, QString())
    LITE_OPTION_ITEM(QString, rmvpePath, QString())


public:
    Property<ParamInfo::Name> defaultForegroundParam = ParamInfo::Breathiness;
    Property<ParamInfo::Name> defaultBackgroundParam = ParamInfo::Tension;

private:
    const QString defaultSingingLanguageKey = "defaultSingLanguage";
    const QString defaultLyricsKey = "defaultLyrics";
    const QString packageSearchPathsKey = "packageSearchPaths";
    // const QString defaultPackageVersionKey = "defaultPackageVersion";
};


#endif // GENERALOPTION_H