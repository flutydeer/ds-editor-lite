#ifndef LANGUAGECOMPATIBILITY_H
#define LANGUAGECOMPATIBILITY_H

#include <lite/ProjectModel/Voice/SingerInfo.h>

#include <QList>
#include <QString>
#include <QStringList>

class Note;

/// Word-level (conversion) language capability and conflict analysis.
///
/// The only inputs are the ordered conversion routes recorded on the singer and
/// the languages the notes declare. Model phoneme/language tables and the
/// application preference never participate.
namespace LanguageCompatibility {

    enum class SupportState {
        /// The ordered route list is authoritative. An empty list is a known
        /// capability ("no addressable conversion route"), not missing data.
        Known,
        /// No singer, unresolved singer metadata, or a failed package/singer
        /// lookup. Callers must not report this as a known empty capability.
        Unresolved,
    };

    struct ConversionSupport {
        SupportState state = SupportState::Unresolved;
        /// Unresolved only: why the routes are unknown.
        QString reason;
        /// Known only: addressable conversion routes in declaration order.
        QStringList languageIds;

        [[nodiscard]] bool isKnown() const;
        [[nodiscard]] bool supports(const QString &language) const;
    };

    /// Notes of one declared language that the candidate singer cannot route.
    struct LanguageConflict {
        QString language;
        QList<int> noteIds;
        /// Subset of noteIds that keep user-edited phonemes.
        QList<int> notesWithEditedPhonemes;
    };

    /// What a voice change would invalidate in the given notes.
    struct RepairImpact {
        /// Notes whose declaration the candidate singer cannot route.
        int conflictedNotes = 0;
        /// Those declarations in clip order, without duplicates.
        QStringList conflictLanguages;
        /// Notes whose edited pronunciation switching clears while keeping declarations.
        int clearedPronunciations = 0;
        /// Total cleared by switching and repairing unsupported declarations to fallback.
        /// Zero when no fallback exists, so repair is unavailable.
        int clearedPronunciationsWithRepair = 0;

        [[nodiscard]] bool isEmpty() const;
    };

    ConversionSupport conversionSupport(const SingerInfo &singer);
    QStringList supportedLanguages(const SingerInfo &singer);

    /// The singer default language when it is addressable, else the first
    /// addressable route, else empty.
    QString fallbackLanguage(const SingerInfo &singer);

    /// The language a group of notes declares most often, when the singer can
    /// route it and no other declared language ties with it. Empty when the
    /// notes declare nothing, split evenly, or only declare unroutable
    /// languages. Fill Lyric uses this as the language the notes already agree
    /// on instead of storing a separate clip-level preference.
    QString dominantLanguage(const QStringList &declaredLanguages,
                             const QStringList &supportedLanguages);

    /// The same preference restricted to the notes Fill Lyrics will rewrite.
    /// `lyrics` and `declaredLanguages` are parallel per-note lists. A slur
    /// carries no syllable of its own, so it does not vote when the caller skips
    /// slurs, even though it keeps its declaration.
    QString dominantLanguage(const QStringList &lyrics, const QStringList &declaredLanguages,
                             bool skipSlur, const QStringList &supportedLanguages);

    /// Groups the specified-language notes the singer cannot route. Notes are
    /// visited in clip order, so the result and every id list are stable.
    QList<LanguageConflict> conflicts(const QList<Note *> &notes, const SingerInfo &singer);

    /// Summarizes conflicts and pronunciation loss when switching to the given
    /// singer, with and without repairing declarations, without touching any note.
    RepairImpact repairImpact(const QList<Note *> &notes, const SingerInfo &singer);

}

#endif // LANGUAGECOMPATIBILITY_H
