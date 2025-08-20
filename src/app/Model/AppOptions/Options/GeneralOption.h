//
// Created by fluty on 24-8-2.
//

#ifndef GENERALOPTION_H
#define GENERALOPTION_H

#include <QStringList>

#include "Global/AppGlobal.h"
#include "Model/AppModel/Params.h"
#include "Model/AppOptions/IOption.h"
#include "Utils/Property.h"

class GeneralOption final : public IOption {
public:
    explicit GeneralOption() : IOption("general"){};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;
    void setPackageSearchPathsAndNotify(QStringList paths);

    QString defaultSingingLanguage = "cmn";
    QString defaultLyric = "啦";
    QStringList packageSearchPaths;
    LITE_OPTION_ITEM(QString, defaultPackage, QString())
    LITE_OPTION_ITEM(QString, defaultSingerId, QString())
    LITE_OPTION_ITEM(QString, defaultSpeakerId, QString())
    LITE_OPTION_ITEM(QString, somePath, QString())
    LITE_OPTION_ITEM(QString, rmvpePath, QString())


public:
    Property<ParamInfo::Name> defaultForegroundParam = ParamInfo::Breathiness;
    Property<ParamInfo::Name> defaultBackgroundParam = ParamInfo::Tension;

private:
    const QString defaultSingingLanguageKey = "defaultSingLanguage";
    const QString defaultLyricKey = "defaultLyric";
    const QString packageSearchPathsKey = "packageSearchPaths";
};


#endif // GENERALOPTION_H