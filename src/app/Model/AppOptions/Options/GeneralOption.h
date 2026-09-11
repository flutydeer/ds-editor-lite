#ifndef GENERALOPTION_H
#define GENERALOPTION_H

// #include <QVersionNumber>

#include <QJsonValue>
#include <QMap>

#include "Global/AppGlobal.h"
#include <lite/ProjectModel/AppModel/Params.h>
#include "Model/AppOptions/IOption.h"
#include <lite/ADT/Property.h>

class GeneralOption final : public IOption {
public:
    explicit GeneralOption() : IOption("general") {};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;
    void setPackageSearchPathsAndNotify(QStringList paths);
    QString defaultLyricForLanguage(const QString &language) const;

    QString defaultSingingLanguage = "cmn";
    QString uiLanguage = "system";
    QMap<QString, QString> defaultLyrics{
        {"cmn", "啦"},
        {"eng", "la"},
        {"jpn", "ら"},
        {"yue", "啦"}
    };
    QStringList packageSearchPaths;
    QStringList recentProjectFiles;
    QJsonValue speakerMixPresets;
    /// Which analyzer to use, named the way an installed one is named:
    /// <package>:analysis/<contribution>.
    ///
    /// A reference rather than a path, because an analyzer is a contribution of an installed
    /// package now and not a file someone downloaded. A path would also stop meaning anything the
    /// day the package is reinstalled somewhere else.
    ///
    /// Empty until someone chooses one. The older keys held filesystem paths and are not read:
    /// there is nothing to migrate them into, since a path does not say which package it came
    /// from or which contract it answers.
    LITE_OPTION_ITEM(QString, noteAnalyzer, QString())
    LITE_OPTION_ITEM(QString, pitchAnalyzer, QString())
    LITE_OPTION_ITEM(QString, libreSVIPPath, QString())


public:
    Property<ParamInfo::Name> defaultForegroundParam = ParamInfo::Breathiness;
    Property<ParamInfo::Name> defaultBackgroundParam = ParamInfo::Tension;

private:
    const QString defaultSingingLanguageKey = "defaultSingLanguage";
    const QString uiLanguageKey = "uiLanguage";
    const QString defaultLyricsKey = "defaultLyrics";
    const QString packageSearchPathsKey = "packageSearchPaths";
    const QString recentProjectFilesKey = "recentProjectFiles";
    const QString speakerMixPresetsKey = "speakerMixPresets";
};


#endif // GENERALOPTION_H
