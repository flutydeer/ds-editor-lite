#include "EditNoteWordPropertiesAction.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/NoteWordCascade.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

EditNoteWordPropertiesAction::EditNoteWordPropertiesAction(
    const QList<Note *> &notes, const QList<Note::WordProperties> &args, SingingClip *clip,
    const QList<WordPropertyEditOptions> &options)
    : m_notes(notes), m_newArgs(args), m_clip(clip) {
    const auto fallbackLanguage = m_clip ? m_clip->fallbackLanguage() : QString();
    const auto resolved = [&fallbackLanguage](const QString &declared) {
        return NoteWordCascade::resolvedLanguage(declared, fallbackLanguage);
    };
    // A word edit that keeps the lyric, the declaration and the resolved
    // language skips G2P and only re-runs S2P, so the notification is narrowed.
    m_pronunciationOnly = true;
    for (int i = 0; i < m_notes.count(); i++) {
        const auto previous = Note::WordProperties::fromNote(*m_notes.at(i));
        m_oldArgs.append(previous);
        const auto noteOptions = i < options.count() ? options.at(i) : WordPropertyEditOptions{};
        NoteWordCascade::Options cascadeOptions;
        cascadeOptions.replacePronunciation = noteOptions.replacePronunciation;
        cascadeOptions.replacePronCandidates = noteOptions.replacePronCandidates;
        auto &next = m_newArgs[i];
        NoteWordCascade::apply(previous, next, resolved(previous.language),
                               resolved(next.language), cascadeOptions);
        if (previous.lyric != next.lyric || previous.language != next.language ||
            resolved(previous.language) != resolved(next.language)) {
            m_pronunciationOnly = false;
        }
    }
}

EditNoteWordPropertiesAction::EditNoteWordPropertiesAction(
    const QList<Note *> &notes, const QList<Note::WordProperties> &applied,
    const QList<QString> &oldResolvedLanguages, const QList<QString> &newResolvedLanguages,
    SingingClip *clip)
    : m_notes(notes), m_newArgs(applied), m_clip(clip) {
    // The caller cascaded against the target language context, so the resolved
    // languages are supplied instead of derived from the current clip.
    m_pronunciationOnly = true;
    for (int i = 0; i < m_notes.count(); i++) {
        const auto previous = Note::WordProperties::fromNote(*m_notes.at(i));
        m_oldArgs.append(previous);
        const auto &next = m_newArgs.at(i);
        if (previous.lyric != next.lyric || previous.language != next.language ||
            oldResolvedLanguages.value(i) != newResolvedLanguages.value(i)) {
            m_pronunciationOnly = false;
        }
    }
}

void EditNoteWordPropertiesAction::execute() {
    const auto previousWordStates = SingingClipPhonemeNormalizer::captureWordStates(*m_clip);
    for (int i = 0; i < m_notes.count(); i++) {
        const auto &[lyric, language, pronunciation, pronCandidates, phonemes] = m_newArgs.at(i);
        const auto note = m_notes.at(i);
        note->setLyric(lyric);
        note->setLanguage(language);
        note->setPhonemes(phonemes);
        note->setPronunciation(Note::Edited, pronunciation.edited);
        note->setPronCandidates(pronCandidates);
    }
    m_resetRecords =
        SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip, previousWordStates);
    m_clip->notifyNoteChanged(m_pronunciationOnly ? SingingClip::EditedPronunciationOnly
                                                  : SingingClip::EditedWordPropertyChange,
                              m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void EditNoteWordPropertiesAction::undo() {
    for (auto i = m_notes.count() - 1; i >= 0; i--) {
        const auto &[lyric, language, pronunciation, pronCandidates, phonemes] =
            m_oldArgs.at(i);
        const auto note = m_notes.at(i);
        note->setLyric(lyric);
        note->setLanguage(language);
        note->setPhonemes(phonemes);
        note->setPronunciation(Note::Edited, pronunciation.edited);
        note->setPronCandidates(pronCandidates);
    }
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyNoteChanged(m_pronunciationOnly ? SingingClip::EditedPronunciationOnly
                                                  : SingingClip::EditedWordPropertyChange,
                              m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
