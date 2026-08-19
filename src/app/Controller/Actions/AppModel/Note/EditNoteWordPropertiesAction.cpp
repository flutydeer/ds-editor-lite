#include "EditNoteWordPropertiesAction.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

namespace {
    bool phonemesEqual(const Phonemes &left, const Phonemes &right) {
        return left.nameSeq.original == right.nameSeq.original &&
               left.nameSeq.edited == right.nameSeq.edited &&
               left.offsetSeq.original == right.offsetSeq.original &&
               left.offsetSeq.edited == right.offsetSeq.edited;
    }

    void applyCascadeResets(const Note::WordProperties &previous, Note::WordProperties &next) {
        next.lyric = next.lyric.trimmed();
        const bool wordInputChanged =
            previous.lyric != next.lyric || previous.language != next.language;
        const bool pronunciationReplaced =
            previous.pronunciation.edited != next.pronunciation.edited;

        if (wordInputChanged) {
            if (!pronunciationReplaced)
                next.pronunciation.edited.clear();
            if (previous.pronCandidates == next.pronCandidates)
                next.pronCandidates.clear();
        }

        auto appliedPronunciation = previous.pronunciation;
        appliedPronunciation.edited = next.pronunciation.edited;
        const bool phonemeInputChanged =
            wordInputChanged || previous.pronunciation.result() != appliedPronunciation.result();
        if (phonemeInputChanged && phonemesEqual(previous.phonemes, next.phonemes))
            next.phonemes = {};
    }
}

EditNoteWordPropertiesAction::EditNoteWordPropertiesAction(const QList<Note *> &notes,
                                                           const QList<Note::WordProperties> &args,
                                                           SingingClip *clip) {
    m_notes = notes;
    m_newArgs = args;
    m_clip = clip;
    // Determine if this is a pronunciation-only edit (lyric and language unchanged).
    // If so, execute()/undo() emit EditedPronunciationOnly so InferController
    // skips G2P and only re-runs S2P.
    m_pronunciationOnly = true;
    for (int i = 0; i < notes.count(); i++) {
        const auto properties = Note::WordProperties::fromNote(*notes.at(i));
        m_oldArgs.append(properties);
        applyCascadeResets(properties, m_newArgs[i]);
        if (properties.lyric != m_newArgs.at(i).lyric ||
            properties.language != m_newArgs.at(i).language) {
            m_pronunciationOnly = false;
        }
    }
}

void EditNoteWordPropertiesAction::execute() {
    const auto previousWordStates =
        SingingClipPhonemeNormalizer::captureWordStates(*m_clip);
    qsizetype i = 0;
    for (const auto note : m_notes) {
        auto [lyric, language, pronunciation, pronCandidates, phonemes] = m_newArgs.at(i);
        note->setLyric(lyric);
        note->setLanguage(language);
        note->setPhonemes(phonemes);
        note->setPronunciation(Note::Edited, pronunciation.edited);
        note->setPronCandidates(pronCandidates);
        i++;
    }
    m_resetRecords =
        SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip, previousWordStates);
    m_clip->notifyNoteChanged(
        m_pronunciationOnly ? SingingClip::EditedPronunciationOnly
                            : SingingClip::EditedWordPropertyChange,
        m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void EditNoteWordPropertiesAction::undo() {
    for (auto i = m_notes.count() - 1; i >= 0; i--) {
        auto [lyric, language, pronunciation, pronCandidates, phonemes] = m_oldArgs.at(i);
        const auto note = m_notes.at(i);
        note->setLyric(lyric);
        note->setLanguage(language);
        note->setPhonemes(phonemes);
        note->setPronunciation(Note::Edited, pronunciation.edited);
        note->setPronCandidates(pronCandidates);
    }
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyNoteChanged(
        m_pronunciationOnly ? SingingClip::EditedPronunciationOnly
                            : SingingClip::EditedWordPropertyChange,
        m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
