#ifndef DEFAULTLYRICRULES_H
#define DEFAULTLYRICRULES_H

#include <QMap>
#include <QString>

/// Shared default-lyric resolution for settings and automation.
/// Singer-declared default lyrics are metadata and are not used here.
namespace DefaultLyricRules {

    /// Built-in values also seed stored profiles. Changing them can turn old defaults into
    /// overrides.
    inline const QMap<QString, QString> &builtinDefaultLyrics() {
        static const QMap<QString, QString> table{
            {QStringLiteral("cmn"), QStringLiteral("啦")},
            {QStringLiteral("eng"), QStringLiteral("la")},
            {QStringLiteral("jpn"), QStringLiteral("ら")},
            {QStringLiteral("yue"), QStringLiteral("啦")},
        };
        return table;
    }

    inline QString builtinDefaultLyricForLanguage(const QString &language) {
        const auto &table = builtinDefaultLyrics();
        if (const auto it = table.constFind(language); it != table.constEnd())
            return it.value();
        return {};
    }

    /// Empty values and values equal to the built-in default are not overrides.
    inline bool isOverride(const QMap<QString, QString> &stored, const QString &language) {
        const auto it = stored.constFind(language);
        if (it == stored.constEnd())
            return false;
        if (it.value().trimmed().isEmpty())
            return false;
        const auto builtin = builtinDefaultLyricForLanguage(language);
        return builtin.isEmpty() || it.value() != builtin;
    }

    /// Resolve user override, then built-in value, then 'la'.
    inline QString resolve(const QMap<QString, QString> &stored, const QString &language) {
        if (isOverride(stored, language))
            return stored.value(language);
        if (const auto builtin = builtinDefaultLyricForLanguage(language); !builtin.isEmpty())
            return builtin;
        return QStringLiteral("la");
    }

}

#endif // DEFAULTLYRICRULES_H
