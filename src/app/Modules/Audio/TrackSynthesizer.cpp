#include "TrackSynthesizer.h"

#include <TalcsCore/Decibels.h>
#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDspx/DspxSingingClipContext.h>
#include <TalcsDspx/DspxNoteContext.h>

#include <Model/AppModel/AppModel.h>
#include <Model/AppModel/Track.h>
#include <Model/AppModel/Clip.h>
#include <Model/AppModel/Note.h>

#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/AudioSettings.h>

#define DEVICE_LOCKER                                                                              \
    talcs::AudioDeviceLocker locker(AudioSystem::outputSystem()->context()->device())

TrackSynthesizer::TrackSynthesizer(talcs::DspxTrackContext *trackContext, Track *track)
    : talcs::DspxPseudoSingerContext(trackContext), m_track(track) {
    talcs::NoteSynthesizerConfig config_;
    config_.setAttackTime(480);
    config_.setDecayTime(48000);
    config_.setDecayRatio(0.5);
    config_.setReleaseTime(2400);
    setConfig(config_);

    for (auto clip : track->clips()) {
        if (clip->clipType() != IClip::Singing)
            continue;
        handleSingingClipInserted(static_cast<SingingClip *>(clip));
    }

    connect(appModel, &AppModel::tempoChanged, this, [=] {
        DEVICE_LOCKER;
        handleTimeChanged();
    });
    connect(AudioSystem::outputSystem()->context(),
            &talcs::AbstractOutputContext::sampleRateChanged, this, [=] {
                DEVICE_LOCKER;
                handleTimeChanged();
            });


    connect(track, &Track::clipChanged, this, [=](Track::ClipChangeType type, Clip *clip) {
        if (clip->clipType() != IClip::Singing)
            return;
        DEVICE_LOCKER;
        switch (type) {
            case Track::Inserted:
                handleSingingClipInserted(static_cast<SingingClip *>(clip));
                break;
            case Track::Removed:
                handleSingingClipRemoved(static_cast<SingingClip *>(clip));
                break;
        }
    });
}

TrackSynthesizer::~TrackSynthesizer() {
}

void TrackSynthesizer::handleSingingClipInserted(SingingClip *clip) {
    auto singingClipContext = addSingingClip(clip->id());
    m_singingClipModelDict.insert(clip, singingClipContext);

    handleSingingClipPropertyChanged(clip);
    for (auto note : clip->notes()) {
        handleNoteInserted(clip, note);
    }

    connect(clip, &Clip::propertyChanged, this, [=] {
        DEVICE_LOCKER;
        handleSingingClipPropertyChanged(clip);
    });
    connect(clip, &SingingClip::noteChanged, this,
            [=](SingingClip::NoteChangeType noteChangeType, const QList<Note *> &notes) {
                DEVICE_LOCKER;
                switch (noteChangeType) {
                    case SingingClip::Insert:
                        for (const auto &note : notes)
                            handleNoteInserted(clip, note);
                        break;
                    case SingingClip::Remove:
                        for (const auto &note : notes)
                            handleNoteRemoved(clip, note);
                        break;
                }
            });
}

void TrackSynthesizer::handleSingingClipRemoved(SingingClip *clip) {
    for (auto note : clip->notes()) {
        handleNoteRemoved(clip, note);
    }
    removeSingingClip(clip->id());
    m_singingClipModelDict.remove(clip);
}

void TrackSynthesizer::handleSingingClipPropertyChanged(SingingClip *clip) {
    auto singingClipContext = m_singingClipModelDict.value(clip);
    singingClipContext->setStart(clip->start());
    singingClipContext->setClipStart(clip->clipStart());
    singingClipContext->setClipLen(clip->clipLen());

    singingClipContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(clip->gain()));
    singingClipContext->controlMixer()->setSilentFlags(clip->mute() ? -1 : 0);
}

void TrackSynthesizer::handleNoteInserted(SingingClip *clip, Note *note) {
    auto singingClipContext = m_singingClipModelDict.value(clip);
    auto noteContext = singingClipContext->addNote(note->id());
    m_noteModelDict.insert(note, noteContext);

    handleNotePropertyChanged(note);

    connect(note, &Note::timeKeyPropertyChanged, this, [=] {
        DEVICE_LOCKER;
        handleNotePropertyChanged(note);
    });
}

void TrackSynthesizer::handleNoteRemoved(SingingClip *clip, Note *note) {
    auto singingClipContext = m_singingClipModelDict.value(clip);
    singingClipContext->removeNote(note->id());
    m_noteModelDict.remove(note);
}

void TrackSynthesizer::handleNotePropertyChanged(Note *note) {
    auto noteContext = m_noteModelDict.value(note);
    noteContext->setKeyCent(note->keyIndex() * 100);
    noteContext->setPos(note->rStart());
    noteContext->setLength(note->length());
}

void TrackSynthesizer::handleTimeChanged() {
    for (auto singingClipContext : clips()) {
        singingClipContext->updatePosition();
        for (auto noteContext : singingClipContext->notes()) {
            noteContext->updatePosition();
        }
    }
}
