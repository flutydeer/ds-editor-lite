#include "NoteWordCascade.h"

namespace NoteWordCascade {
    QString resolvedLanguage(const QString &declared, const QString &fallback) {
        return declared.isEmpty() ? fallback : declared;
    }

    void apply(const Note::WordProperties &previous, Note::WordProperties &next,
               const QString &previousResolvedLanguage, const QString &nextResolvedLanguage,
               const Options &options) {
        next.lyric = next.lyric.trimmed();

        const bool lyricChanged = previous.lyric != next.lyric;
        const bool languageChanged = previous.language != next.language ||
                                     previousResolvedLanguage != nextResolvedLanguage;
        const bool wordInputChanged = lyricChanged || languageChanged;

        if (wordInputChanged) {
            if (!options.replacePronunciation)
                next.pronunciation.edited.clear();
            if (!options.replacePronCandidates)
                next.pronCandidates.clear();
        }

        if (lyricChanged) {
            next.phonemes = {};
        } else if (languageChanged) {
            // Manual names can span languages, but derived names describe the old route.
            // Edited timing needs a surviving name for every offset.
            next.phonemes.nameSeq.original.clear();
            next.phonemes.offsetSeq.original.clear();
            if (next.phonemes.offsetSeq.edited.count() != next.phonemes.nameSeq.result().count())
                next.phonemes.offsetSeq.edited.clear();
        }
    }

    QList<FallbackChange> planFallbackChange(const QList<Note *> &notes,
                                             const QString &previousFallback,
                                             const QString &nextFallback) {
        QList<FallbackChange> plans;
        if (previousFallback == nextFallback)
            return plans;
        for (auto *note : notes) {
            const auto previous = Note::WordProperties::fromNote(*note);
            const auto previousResolved = resolvedLanguage(previous.language, previousFallback);
            const auto nextResolved = resolvedLanguage(previous.language, nextFallback);
            if (previousResolved == nextResolved)
                continue;
            FallbackChange plan{.note = note,
                                .next = previous,
                                .previousResolvedLanguage = previousResolved,
                                .nextResolvedLanguage = nextResolved};
            apply(previous, plan.next, previousResolved, nextResolved);
            plans.append(std::move(plan));
        }
        return plans;
    }

    void applyFallbackChange(const QList<Note *> &notes, const QString &previousFallback,
                             const QString &nextFallback) {
        for (const auto &plan : planFallbackChange(notes, previousFallback, nextFallback)) {
            const auto &next = plan.next;
            auto *note = plan.note;
            note->setLyric(next.lyric);
            note->setLanguage(next.language);
            note->setPhonemes(next.phonemes);
            note->setPronunciation(Note::Edited, next.pronunciation.edited);
            note->setPronCandidates(next.pronCandidates);
        }
    }
}


