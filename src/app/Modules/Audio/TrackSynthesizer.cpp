#include "TrackSynthesizer.h"

#include "AudioContext.h"
#include "../../../libs/talcs/src/dspx/DspxTrackContext.h"
#include "Model/AppModel/SingingClip.h"

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
#include <Modules/Audio/utils/PseudoSingerConfigNotifier.h>

#define DEVICE_LOCKER                                                                              \
    talcs::AudioDeviceLocker locker(AudioSystem::outputSystem()->context()->device())

TrackSynthesizer::TrackSynthesizer(talcs::DspxTrackContext *trackContext, Track *track)
    : DspxPseudoSingerContext(trackContext), m_track(track) {
    setConfig(PseudoSingerConfigNotifier::config(0));
    connect(PseudoSingerConfigNotifier::instance(), &PseudoSingerConfigNotifier::configChanged,
            this, [this](const int synthIndex, const talcs::NoteSynthesizerConfig &config) {
                if (synthIndex == 0)
                    setConfig(config);
            });

    for (const auto clip : track->clips()) {
        if (clip->clipType() != IClip::Singing)
            continue;
        handleSingingClipInserted(static_cast<SingingClip *>(clip));
    }

    connect(appModel, &AppModel::tempoChanged, this, [this] {
        DEVICE_LOCKER;
        handleTimeChanged();
    });
    connect(AudioSystem::outputSystem()->context(),
            &talcs::AbstractOutputContext::sampleRateChanged, this, [this] {
                DEVICE_LOCKER;
                handleTimeChanged();
            });

    connect(static_cast<AudioContext *>(trackContext->projectContext()),
            &AudioContext::exporterCausedTimeChanged, this, &TrackSynthesizer::handleTimeChanged);

    connect(track, &Track::clipChanged, this, [this](const Track::ClipChangeType type, Clip *clip) {
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
    const auto singingClipContext = addSingingClip(clip->id());
    m_singingClipModelDict.insert(clip, singingClipContext);

    handleSingingClipPropertyChanged(clip);
    for (const auto note : clip->notes()) {
        handleNoteInserted(clip, note);
    }

    connect(clip, &Clip::propertyChanged, this, [clip, this] {
        DEVICE_LOCKER;
        handleSingingClipPropertyChanged(clip);
    });
    connect(
        clip, &SingingClip::noteChanged, this,
        [clip, this](const SingingClip::NoteChangeType noteChangeType, const QList<Note *> &notes) {
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
                case SingingClip::TimeKeyPropertyChange:
                    for (const auto &note : notes)
                        handleNotePropertyChanged(note);
                default:
                    break;
            }
        });
}

void TrackSynthesizer::handleSingingClipRemoved(SingingClip *clip) {
    for (const auto note : clip->notes()) {
        handleNoteRemoved(clip, note);
    }
    disconnect(clip, nullptr, this, nullptr);
    removeSingingClip(clip->id());
    m_singingClipModelDict.remove(clip);
}

void TrackSynthesizer::handleSingingClipPropertyChanged(SingingClip *clip) const {
    const auto singingClipContext = m_singingClipModelDict.value(clip);
    singingClipContext->setStart(clip->start());
    singingClipContext->setClipStart(clip->clipStart());
    singingClipContext->setClipLen(clip->clipLen());

    singingClipContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(clip->gain()));
    singingClipContext->controlMixer()->setSilentFlags(clip->mute() ? -1 : 0);
}

void TrackSynthesizer::handleNoteInserted(SingingClip *clip, Note *note) {
    const auto singingClipContext = m_singingClipModelDict.value(clip);
    const auto noteContext = singingClipContext->addNote(note->id());
    m_noteModelDict.insert(note, noteContext);

    handleNotePropertyChanged(note);
}

void TrackSynthesizer::handleNoteRemoved(SingingClip *clip, Note *note) {
    disconnect(note, nullptr, this, nullptr);
    const auto singingClipContext = m_singingClipModelDict.value(clip);
    singingClipContext->removeNote(note->id());
    m_noteModelDict.remove(note);
}

void TrackSynthesizer::handleNotePropertyChanged(Note *note) const {
    const auto noteContext = m_noteModelDict.value(note);
    noteContext->setKeyCent(note->keyIndex() * 100);
    noteContext->setPos(note->localStart());
    noteContext->setLength(note->length());
}

void TrackSynthesizer::handleTimeChanged() const {
    for (const auto singingClipContext : clips()) {
        singingClipContext->updatePosition();
        for (const auto noteContext : singingClipContext->notes()) {
            noteContext->updatePosition();
        }
    }
}
