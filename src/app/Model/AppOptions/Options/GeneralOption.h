#ifndef GENERALOPTION_H
#define GENERALOPTION_H

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

    /// Resolve the user override, then the built-in default, then 'la'.
    QString defaultLyricForLanguage(const QString &language) const;

    /// Empty values and values equal to the built-in default are not overrides.
    bool hasDefaultLyricOverride(const QString &language) const;

    static QString builtinDefaultLyricForLanguage(const QString &language);

    static const QMap<QString, QString> &builtinDefaultLyrics();

    QString defaultSingingLanguage = "cmn";
    QString uiLanguage = "system";
    QMap<QString, QString> defaultLyrics = builtinDefaultLyrics();
    QStringList packageSearchPaths;
    QStringList recentProjectFiles;
    QJsonValue speakerMixPresets;
    // Whether a finger may draw parameter curves in the clip editor. Off by
    // default: a finger on the parameter panel (or with a pitch tool armed in
    // the piano roll) scrolls the timeline instead, and the tool belongs to the
    // pen and the mouse. The speaker mix editor is not covered.
    LITE_OPTION_ITEM(bool, drawParamWithFinger, false)
    /// Selected analyzer reference: package:inference/contribution, empty until selected.
    /// Legacy analysis-category references are normalized on load; filesystem paths are not
    /// migrated.
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
