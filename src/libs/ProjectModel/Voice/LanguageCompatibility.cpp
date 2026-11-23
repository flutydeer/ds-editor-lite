#include <lite/ProjectModel/Voice/LanguageCompatibility.h>

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/NoteWordCascade.h>

#include <QCoreApplication>
#include <QHash>

namespace LanguageCompatibility {

    namespace {
        QString translated(const char *text) {
            return QCoreApplication::translate("LanguageCompatibility", text);
        }
    }

    bool ConversionSupport::isKnown() const {
        return state == SupportState::Known;
    }

    bool ConversionSupport::supports(const QString &language) const {
        return state == SupportState::Known && languageIds.contains(language);
    }

    ConversionSupport conversionSupport(const SingerInfo &singer) {
        if (singer.isEmpty())
            return {SupportState::Unresolved,
                    translated(QT_TRANSLATE_NOOP("LanguageCompatibility", "No singer selected")),
                    {}};

        switch (singer.resolutionState()) {
            case ResolutionState::Resolved: {
                // A populated projection is authoritative, including the empty
                // case where no declared language has a usable route. A document
                // round trip or a fixture carries no projection, so the declared
                // languages are the only candidates then.
                if (singer.hasConversionLanguages()) {
                    const auto projected = singer.conversionLanguages();
                    if (projected.isEmpty()) {
                        return {SupportState::Known,
                                translated(QT_TRANSLATE_NOOP(
                                    "LanguageCompatibility",
                                    "The singer provides no conversion route")),
                                {}};
                    }
                    return {SupportState::Known, {}, projected};
                }
                QStringList declared;
                for (const auto &languageInfo : singer.languages())
                    declared.append(languageInfo.id());
                if (declared.isEmpty()) {
                    return {SupportState::Unresolved,
                            translated(QT_TRANSLATE_NOOP(
                                "LanguageCompatibility",
                                "The singer declares no conversion language")),
                            {}};
                }
                return {SupportState::Known, {}, declared};
            }
            case ResolutionState::Pending:
                return {SupportState::Unresolved,
                        translated(QT_TRANSLATE_NOOP("LanguageCompatibility",
                                                     "Singer metadata is not ready")),
                        {}};
            case ResolutionState::Missing:
                return {SupportState::Unresolved,
                        translated(QT_TRANSLATE_NOOP("LanguageCompatibility",
                                                     "The singer package is not installed")),
                        {}};
        }
        return {SupportState::Unresolved,
                translated(QT_TRANSLATE_NOOP("LanguageCompatibility",
                                             "Singer metadata is not ready")),
                {}};
    }

    bool RepairImpact::isEmpty() const {
        return conflictedNotes == 0 && clearedPronunciations == 0 &&
               clearedPronunciationsWithRepair == 0;
    }

    RepairImpact repairImpact(const QList<Note *> &notes, const SingerInfo &singer) {
        RepairImpact result;
        for (const auto &conflict : conflicts(notes, singer)) {
            result.conflictedNotes += static_cast<int>(conflict.noteIds.size());
            if (!result.conflictLanguages.contains(conflict.language))
                result.conflictLanguages.append(conflict.language);
        }
        const auto targetLanguage = fallbackLanguage(singer);
        for (const auto note : notes) {
            if (!note || note->pronunciation().edited.isEmpty())
                continue;
            const auto nextResolved =
                NoteWordCascade::resolvedLanguage(note->language(), targetLanguage);
            const bool resolvedLanguageChanged = note->resolvedLanguage() != nextResolved;
            if (resolvedLanguageChanged)
                result.clearedPronunciations++;
            if (!targetLanguage.isEmpty() &&
                (resolvedLanguageChanged || result.conflictLanguages.contains(note->language())))
                result.clearedPronunciationsWithRepair++;
        }
        return result;
    }

    QStringList supportedLanguages(const SingerInfo &singer) {
        return conversionSupport(singer).languageIds;
    }

    QString fallbackLanguage(const SingerInfo &singer) {
        const auto support = conversionSupport(singer);
        if (!support.isKnown() || support.languageIds.isEmpty())
            return {};
        const auto preferred = singer.defaultLanguage();
        if (!preferred.isEmpty() && support.languageIds.contains(preferred))
            return preferred;
        return support.languageIds.first();
    }

    QString dominantLanguage(const QStringList &declaredLanguages,
                             const QStringList &supportedLanguages) {
        // Only routable declarations are candidates, so an unroutable majority
        // never wins. A tie stays empty: mixed content is better served by
        // detecting each token.
        qsizetype bestCount = 0;
        bool tied = false;
        QString best;
        for (const auto &language : supportedLanguages) {
            const auto count = declaredLanguages.count(language);
            if (count == 0)
                continue;
            if (count > bestCount) {
                bestCount = count;
                best = language;
                tied = false;
            } else if (count == bestCount) {
                tied = true;
            }
        }
        return tied ? QString() : best;
    }

    QList<LanguageConflict> conflicts(const QList<Note *> &notes, const SingerInfo &singer) {
        const auto support = conversionSupport(singer);
        if (!support.isKnown())
            return {};

        QList<LanguageConflict> result;
        QHash<QString, qsizetype> indexByLanguage;
        for (const auto note : notes) {
            if (!note || note->languageState() != NoteLanguageState::Specified)
                continue;
            const auto language = note->language();
            if (support.languageIds.contains(language))
                continue;

            auto index = indexByLanguage.value(language, -1);
            if (index < 0) {
                index = result.size();
                indexByLanguage.insert(language, index);
                result.append(LanguageConflict{language, {}, {}});
            }
            result[index].noteIds.append(note->id());
            if (note->phonemeNameSeq().isEdited())
                result[index].notesWithEditedPhonemes.append(note->id());
        }
        return result;
    }

}
