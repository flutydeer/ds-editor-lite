#ifndef NOTEWORDCASCADE_H
#define NOTEWORDCASCADE_H

#include <lite/ProjectModel/AppModel/Note.h>

#include <QString>

/// Resets of the derived word data after a lyric, language or resolved-language
/// change.
///
/// Every path that rewrites note word properties shares this cascade so that an
/// edit, a voice-context switch and a cross-track move invalidate the same
/// things.
namespace NoteWordCascade {
    struct Options {
        /// Keep the manually entered pronunciation of the incoming properties.
        bool replacePronunciation = false;
        /// Keep the incoming pronunciation candidate list.
        bool replacePronCandidates = false;
    };

    /// The language a note resolves to when it carries no declaration.
    [[nodiscard]] QString resolvedLanguage(const QString &declared, const QString &fallback);

    /// Applies the cascade to `next` using `previous` as the applied state.
    ///
    /// A lyric change, a declaration change or a change of the resolved language
    /// invalidates the edited pronunciation, the candidates and the phoneme
    /// sequence. Manually edited phoneme names survive: the compatibility
    /// diagnostics report the ones the voice source cannot synthesize, so the
    /// user keeps the chance to adjust them instead of losing the work.
    void apply(const Note::WordProperties &previous, Note::WordProperties &next,
               const QString &previousResolvedLanguage, const QString &nextResolvedLanguage,
               const Options &options = {});

    /// One note that a route change invalidates.
    struct FallbackChange {
        Note *note = nullptr;
        Note::WordProperties next;
        QString previousResolvedLanguage;
        QString nextResolvedLanguage;
    };

    /// Plans the cascade for every note whose resolved language changes when the
    /// surrounding route moves from `previousFallback` to `nextFallback`.
    [[nodiscard]] QList<FallbackChange> planFallbackChange(const QList<Note *> &notes,
                                                           const QString &previousFallback,
                                                           const QString &nextFallback);

    /// Applies `planFallbackChange()` in place.
    ///
    /// Used while building clips that are not part of the document yet, such as
    /// cloning a clip onto another track. Paths that edit a loaded document
    /// commit the planned properties through the normal note action instead.
    void applyFallbackChange(const QList<Note *> &notes, const QString &previousFallback,
                             const QString &nextFallback);
}

#endif // NOTEWORDCASCADE_H
