#ifndef DEFAULTLYRICRULES_H
#define DEFAULTLYRICRULES_H

#include <QMap>
#include <QString>

/// The default lyric of a language, in one place.
///
/// The editor resolves a default lyric from a user setting and from the built-in value below. The
/// rules live in a header because two layers that cannot share a translation unit both need them:
/// the settings of the application and the automation surface, which compiles without linking the
/// preferences core. Keeping one implementation is what makes the two paths agree on every state of
/// the stored map, including a missing key and an empty value.
///
/// A voicebank states a default lyric per language in its linguist module. The editor reads that
/// statement (see VoicebankCatalog, LanguageInfo and SingerInfo::defaultLyricForLanguage), but the
/// resolution below deliberately does not consume it: it stays driven by the settings and by the
/// built-in table, so the reading can be reviewed before anything depends on it.
namespace DefaultLyricRules {

    /// Returns the built-in default lyric of every language the editor ships a value for.
    ///
    /// The table is a fallback, never a user setting: the stored map starts as a copy of it only so
    /// that a profile written by an older build keeps its shape, and isOverride() ignores the
    /// entries that still equal it. For that reason a value in this table must keep its meaning
    /// forever. Changing one would turn the copy in every existing profile into a user override.
    inline const QMap<QString, QString> &builtinDefaultLyrics() {
        static const QMap<QString, QString> table{
            {QStringLiteral("cmn"), QStringLiteral("啦")},
            {QStringLiteral("eng"), QStringLiteral("la")},
            {QStringLiteral("jpn"), QStringLiteral("ら")},
            {QStringLiteral("yue"), QStringLiteral("啦")},
        };
        return table;
    }

    /// Returns the built-in default lyric of \a language, or an empty string when none is known.
    inline QString builtinDefaultLyricForLanguage(const QString &language) {
        const auto &table = builtinDefaultLyrics();
        if (const auto it = table.constFind(language); it != table.constEnd())
            return it.value();
        return {};
    }

    /// Returns whether \a stored overrides the default lyric of \a language.
    ///
    /// An empty stored value means "use the default" rather than "use an empty lyric", and a value
    /// that equals the built-in value of that language is what the settings shipped with rather
    /// than a choice, so neither is an override.
    inline bool isOverride(const QMap<QString, QString> &stored, const QString &language) {
        const auto it = stored.constFind(language);
        if (it == stored.constEnd())
            return false;
        if (it.value().trimmed().isEmpty())
            return false;
        const auto builtin = builtinDefaultLyricForLanguage(language);
        return builtin.isEmpty() || it.value() != builtin;
    }

    /// Resolves the default lyric of \a language.
    ///
    /// Precedence: a user override wins, then the built-in value of that language, then "la" for a
    /// language nothing knows about.
    inline QString resolve(const QMap<QString, QString> &stored, const QString &language) {
        if (isOverride(stored, language))
            return stored.value(language);
        if (const auto builtin = builtinDefaultLyricForLanguage(language); !builtin.isEmpty())
            return builtin;
        return QStringLiteral("la");
    }

}

#endif // DEFAULTLYRICRULES_H
