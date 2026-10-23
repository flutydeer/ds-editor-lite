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

    /// Resolves the default lyric a new note receives in \a language.
    ///
    /// Precedence: a user override in the settings wins, then \a declared, which is the value an
    /// installed singer states for that language, then the built-in value, then "la" for a language
    /// nothing knows about. A stored value that equals the built-in value of that language is not
    /// an override, because the settings ship with those values and they must not shadow a singer.
    QString defaultLyricForLanguage(const QString &language) const;

    /// Returns whether the user overrode the default lyric of \a language.
    ///
    /// An empty stored value means "use the default" rather than "use an empty lyric", so it is not
    /// an override either.
    bool hasDefaultLyricOverride(const QString &language) const;

    /// Returns the built-in default lyric of \a language, or an empty string when none is known.
    static QString builtinDefaultLyricForLanguage(const QString &language);

    /// Returns the built-in default lyric of every language the editor ships a value for.
    ///
    /// This table is a fallback, never a user setting: the stored map starts as a copy of it only
    /// so that a profile written by an older build keeps its shape, and hasDefaultLyricOverride()
    /// ignores the entries that still equal it.
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
    /// Reference to the selected analyzer, in the form of an installed contribution:
    /// <package>:inference/<contribution>. A reference in the legacy
    /// <package>:analysis/<contribution> form is upgraded when the options are loaded.
    ///
    /// The option stores a reference rather than a path because an analyzer is a contribution of
    /// an installed package, not a separately downloaded file. A path would also become invalid if
    /// the package is reinstalled at another location.
    ///
    /// Empty until the user selects an analyzer. The legacy keys stored filesystem paths and are
    /// not read, because a path identifies neither the package nor the contract of the analyzer,
    /// and therefore cannot be migrated to a reference.
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
