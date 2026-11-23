#include "MoveClipToTrackAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/NoteWordCascade.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/Voice/LanguageCompatibility.h>

#include <optional>

namespace {
    void applyArgs(Clip *clip, const Clip::ClipCommonProperties &args, const bool updateAudioTiming,
                   const bool preserveAudioTickCaches) {
        clip->setName(args.name);
        clip->setStart(args.start);
        clip->setClipStart(args.clipStart);
        clip->setLength(args.length);
        clip->setClipLen(args.clipLen);
        clip->setGain(args.gain);
        clip->setMute(args.mute);
        if (clip->clipType() == IClip::Audio && updateAudioTiming) {
            static_cast<AudioClip *>(clip)->applyRealTimeAnchorFromProperties(
                args, appModel->timeline(), !preserveAudioTickCaches);
        }
    }
}

MoveClipToTrackAction *MoveClipToTrackAction::build(const Clip::ClipCommonProperties &oldArgs,
                                                    const Clip::ClipCommonProperties &newArgs,
                                                    Clip *clip, Track *oldTrack, Track *newTrack) {
    const auto a = new MoveClipToTrackAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    a->m_oldTrack = oldTrack;
    a->m_newTrack = newTrack;
    if (clip->clipType() == IClip::Audio) {
        const auto audioClip = static_cast<AudioClip *>(clip);
        const auto &timeline = appModel->timeline();
        if (a->m_oldArgs.playLengthMs < 0) {
            if (audioClip->hasRealTimeAnchor()) {
                a->m_oldArgs.trimStartMs = audioClip->trimStartMs();
                a->m_oldArgs.playLengthMs = audioClip->playLengthMs();
                a->m_oldArgs.materialLengthMs = audioClip->materialLengthMs();
            } else {
                AudioClip::deriveTruthForProperties(a->m_oldArgs, timeline);
            }
        }
        if (a->m_newArgs.playLengthMs < 0) {
            AudioClip::deriveTruthForProperties(a->m_newArgs, timeline);
            AudioClip::preserveUnchangedTruth(a->m_newArgs, a->m_oldArgs);
        }
        a->m_audioTimingChanged = !AudioClip::timingPropertiesEqual(a->m_oldArgs, a->m_newArgs);
    } else if (clip->clipType() == IClip::Singing) {
        const auto singing = static_cast<SingingClip *>(clip);
        // Only a clip that follows its track changes its resolved singer. The
        // cascade is precomputed here because after the move the old track no
        // longer describes the previous state.
        if (singing->usesTrackVoiceContext()) {
            // An unresolved route is unknown, not an authoritative empty one, so
            // it says nothing about the language the notes were resolved for.
            const auto oldFallback =
                LanguageCompatibility::conversionSupport(oldTrack->singerInfo()).isKnown()
                    ? LanguageCompatibility::fallbackLanguage(oldTrack->singerInfo())
                    : std::optional<QString>{};
            const auto newFallback =
                LanguageCompatibility::conversionSupport(newTrack->singerInfo()).isKnown()
                    ? LanguageCompatibility::fallbackLanguage(newTrack->singerInfo())
                    : std::optional<QString>{};
            if (oldFallback && newFallback && *oldFallback != *newFallback) {
                for (const auto note : singing->notes()) {
                    const auto previous = Note::WordProperties::fromNote(*note);
                    const auto oldResolved =
                        NoteWordCascade::resolvedLanguage(previous.language, *oldFallback);
                    const auto newResolved =
                        NoteWordCascade::resolvedLanguage(previous.language, *newFallback);
                    if (oldResolved == newResolved)
                        continue;
                    auto next = previous;
                    NoteWordCascade::apply(previous, next, oldResolved, newResolved);
                    a->m_cascadedNotes.append(note);
                    a->m_oldWordProperties.append(previous);
                    a->m_newWordProperties.append(next);
                }
            }
        }
    }
    return a;
}

void MoveClipToTrackAction::applyWordProperties(const QList<Note::WordProperties> &properties) {
    for (int i = 0; i < m_cascadedNotes.count(); i++) {
        const auto &[lyric, language, pronunciation, pronCandidates, phonemes] = properties.at(i);
        const auto note = m_cascadedNotes.at(i);
        note->setLyric(lyric);
        note->setLanguage(language);
        note->setPhonemes(phonemes);
        note->setPronunciation(Note::Edited, pronunciation.edited);
        note->setPronCandidates(pronCandidates);
    }
}

void MoveClipToTrackAction::execute() {
    SingingClipPhonemeNormalizer::WordStates previousWordStates;
    if (m_clip->clipType() == IClip::Singing)
        previousWordStates =
            SingingClipPhonemeNormalizer::captureWordStates(*static_cast<SingingClip *>(m_clip));
    m_oldTrack->removeClip(m_clip);
    applyArgs(m_clip, m_newArgs, m_audioTimingChanged, false);
    m_newTrack->insertClip(m_clip);
    if (m_clip->clipType() == IClip::Singing) {
        const auto singingClip = static_cast<SingingClip *>(m_clip);
        singingClip->setTrackVoiceContext(m_newTrack->singerInfo(), m_newTrack->speakerInfo(),
                                          m_newTrack->speakerMixData());
        applyWordProperties(m_newWordProperties);
        m_resetRecords = previousWordStates.isEmpty()
                             ? QList<SingingClipPhonemeNormalizer::ResetRecord>{}
                             : SingingClipPhonemeNormalizer::normalizeEditedOffsets(
                                   *singingClip, previousWordStates);
    } else {
        m_resetRecords.clear();
    }
    m_clip->notifyPropertyChanged();
    if (!m_cascadedNotes.isEmpty()) {
        static_cast<SingingClip *>(m_clip)->notifyNoteChanged(
            SingingClip::EditedWordPropertyChange, m_cascadedNotes);
    }
    if (!m_resetRecords.isEmpty()) {
        const auto singingClip = static_cast<SingingClip *>(m_clip);
        singingClip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
    }
    m_oldTrack->notifyClipChanged(Track::Removed, m_clip);
    m_newTrack->notifyClipChanged(Track::Inserted, m_clip);
}

void MoveClipToTrackAction::undo() {
    m_newTrack->removeClip(m_clip);
    applyArgs(m_clip, m_oldArgs, m_audioTimingChanged, true);
    m_oldTrack->insertClip(m_clip);
    if (m_clip->clipType() == IClip::Singing) {
        const auto singingClip = static_cast<SingingClip *>(m_clip);
        singingClip->setTrackVoiceContext(m_oldTrack->singerInfo(), m_oldTrack->speakerInfo(),
                                          m_oldTrack->speakerMixData());
        applyWordProperties(m_oldWordProperties);
        SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    }
    m_clip->notifyPropertyChanged();
    if (!m_cascadedNotes.isEmpty()) {
        static_cast<SingingClip *>(m_clip)->notifyNoteChanged(
            SingingClip::EditedWordPropertyChange, m_cascadedNotes);
    }
    if (!m_resetRecords.isEmpty()) {
        const auto singingClip = static_cast<SingingClip *>(m_clip);
        singingClip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
    }
    m_newTrack->notifyClipChanged(Track::Removed, m_clip);
    m_oldTrack->notifyClipChanged(Track::Inserted, m_clip);
}
