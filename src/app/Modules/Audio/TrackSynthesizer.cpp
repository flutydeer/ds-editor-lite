#include "TrackSynthesizer.h"

#include <limits>

#include <TalcsMidi/MidiMessage.h>

#include <Model/AppModel/AppModel.h>
#include <Model/AppModel/Track.h>
#include <Modules/Audio/AudioSettings.h>
#include <Modules/Audio/utils/AudioHelpers.h>

static constexpr qint64 tickToSample(double tick, double sampleRate, double tempo) {
    return qint64(tick * 60.0 * sampleRate / tempo / 480.0);
}

static constexpr double sampleToTick(qint64 sample, double sampleRate, double tempo) {
    return double(sample) / sampleRate * tempo / 60.0 * 480.0;
}

static const char MAGIC_PROP[] = "TrackSynthesizer.Replica";

Note *TrackSynthesizer::replicateNote(Note *note, SingingClip *singingClip) {
    auto replica = new Note;
    note->setProperty(MAGIC_PROP, QVariant::fromValue(replica));
    replica->setParent(singingClip);
    replica->setStart(note->start());
    replica->setLength(note->length());
    replica->setKeyIndex(note->keyIndex());
    connect(note, &Note::propertyChanged, replica, [=](Note::NotePropertyType type) {
        if (type != Note::TimeAndKey)
            return;
        QMutexLocker locker(&m_mutex);
        singingClip->removeNote(replica);
        replica->setStart(note->start());
        replica->setLength(note->length());
        replica->setKeyIndex(note->keyIndex());
        singingClip->insertNote(replica);
    });
    return replica;
}

SingingClip *TrackSynthesizer::replicateSingingClip(SingingClip *singingClip, Track *track) {
    auto replica = new SingingClip;
    singingClip->setProperty(MAGIC_PROP, QVariant::fromValue(replica));
    replica->setParent(track);
    replica->setStart(singingClip->start());
    replica->setClipStart(singingClip->clipStart());
    replica->setClipLen(singingClip->clipLen());
    for (auto note : singingClip->notes()) {
        replica->insertNote(replicateNote(note, replica));
    }
    QObject::connect(singingClip, &Clip::propertyChanged, replica, [=] {
        QMutexLocker locker(&m_mutex);
        track->removeClip(replica);
        replica->setStart(singingClip->start());
        replica->setClipStart(singingClip->clipStart());
        replica->setClipLen(singingClip->clipLen());
        track->insertClip(replica);
    });
    QObject::connect(singingClip, &SingingClip::noteChanged, replica, [=](SingingClip::NoteChangeType type, Note *note) {
        QMutexLocker locker(&m_mutex);
        if (type == SingingClip::Inserted) {
            replica->insertNote(replicateNote(note, replica));
        } else {
            auto replicatedNote = note->property(MAGIC_PROP).value<Note *>();
            replica->removeNote(replicatedNote);
            delete replicatedNote;
            note->setProperty(MAGIC_PROP, {});
        }
    });
    return replica;
}

Track *TrackSynthesizer::replicateTrack(Track *track, QObject *parent) {
    auto replica = new Track;
    replica->setParent(parent);
    for (auto clip : track->clips()) {
        if (clip->clipType() == IClip::Singing) {
            auto singingClip = static_cast<SingingClip *>(clip);
            replica->insertClip(replicateSingingClip(singingClip, replica));
        }
    }
    QObject::connect(track, &Track::clipChanged, replica, [=](Track::ClipChangeType type, Clip *clip) {
        if (clip->clipType() != IClip::Singing)
            return;
        QMutexLocker locker(&m_mutex);
        auto singingClip = static_cast<SingingClip *>(clip);
        if (type == Track::Inserted) {
            replica->insertClip(replicateSingingClip(singingClip, replica));
        } else {
            auto replicatedSingingClip = clip->property(MAGIC_PROP).value<SingingClip *>();
            replica->removeClip(replicatedSingingClip);
            delete replicatedSingingClip;
            singingClip->setProperty(MAGIC_PROP, {});
        }
    });
    return replica;
}

TrackSynthesizer::TrackSynthesizer(Track *track) : m_track(track), m_synthesizer(new talcs::NoteSynthesizer) {
    m_synthesizer->setGenerator(talcs::NoteSynthesizer::Sine);
    m_synthesizer->setDetector(this);

    m_replicaTrack = replicateTrack(track, this);

}
TrackSynthesizer::~TrackSynthesizer() {
}

bool TrackSynthesizer::open(qint64 bufferSize, double sampleRate) {
    m_synthesizer->setAttackTime(AudioHelpers::msecToSample(10, sampleRate));
    m_synthesizer->setDecayTime(AudioHelpers::msecToSample(500, sampleRate));
    m_synthesizer->setDecayRatio(0.5);
    m_synthesizer->setReleaseTime(AudioHelpers::msecToSample(50, sampleRate));
    m_synthesizer->open(bufferSize, sampleRate);
    return AudioSource::open(bufferSize, sampleRate);
}
void TrackSynthesizer::close() {
    m_synthesizer->close();
    AudioSource::close();
}
qint64 TrackSynthesizer::length() const {
    return std::numeric_limits<qint64>::max();
}
qint64 TrackSynthesizer::nextReadPosition() const {
    return PositionableAudioSource::nextReadPosition();
}
void TrackSynthesizer::setNextReadPosition(qint64 pos) {
    if (pos == PositionableAudioSource::nextReadPosition())
        return;
    m_synthesizer->flush(true);
    PositionableAudioSource::setNextReadPosition(pos);
}
qint64 TrackSynthesizer::processReading(const talcs::AudioSourceReadData &readData) {
    m_synthesizer->read(readData);
    PositionableAudioSource::setNextReadPosition(PositionableAudioSource::nextReadPosition() + readData.length);
    return readData.length;
}
void TrackSynthesizer::detectInterval(qint64 intervalLength) {
    QMutexLocker locker(&m_mutex);
    auto currentPosition = PositionableAudioSource::nextReadPosition();
    auto tempo = appModel->tempo();
    auto sr = sampleRate();
    auto aFreq = AudioSettings::midiSynthesizerFrequencyOfA();
    aFreq = qFuzzyIsNull(aFreq) ? 440.0 : aFreq;

    auto intervalStart = static_cast<qsizetype>(sampleToTick(currentPosition, sr, tempo));
    auto intervalEnd = static_cast<qsizetype>(sampleToTick(currentPosition + intervalLength, sr, tempo));

    auto overlappedClips = m_replicaTrack->clips().findOverlappedItems({intervalStart, intervalEnd});
    auto clipIt = std::find_if(overlappedClips.cbegin(), overlappedClips.cend(), [](Clip *clip) {
        return clip->clipType() == IClip::Singing;
    });
    if (clipIt == overlappedClips.end())
        return;
    auto overlappedNotes = static_cast<SingingClip *>(*clipIt)->notes().findOverlappedItems({
        std::max<qsizetype>(intervalStart - (*clipIt)->start(), (*clipIt)->clipStart()),
        intervalEnd - (*clipIt)->start(),
    });

    m_messages.clear();
    for (auto note : overlappedNotes) {
        auto noteStartPosition = tickToSample(note->start(), sr, tempo) - currentPosition ;
        auto noteEndPosition = tickToSample(note->start() + note->length(), sr, tempo) - currentPosition;
        if (noteStartPosition < 0) {
            m_messages.append({0, talcs::MidiMessage::getMidiNoteInHertz(note->keyIndex(), aFreq), 0.5, talcs::NoteSynthesizerDetectorMessage::NoteOnIfNotPlaying});
        } else {
            m_messages.append({noteStartPosition, talcs::MidiMessage::getMidiNoteInHertz(note->keyIndex(), aFreq), 0.5, talcs::NoteSynthesizerDetectorMessage::NoteOn});
        }

        if (noteEndPosition < intervalLength) {
            m_messages.append({noteEndPosition, talcs::MidiMessage::getMidiNoteInHertz(note->keyIndex(), aFreq), talcs::NoteSynthesizerDetectorMessage::NoteOff});
        }
    }
    m_messageIterator = m_messages.cbegin();
}
talcs::NoteSynthesizerDetectorMessage TrackSynthesizer::nextMessage() {
    if (m_messageIterator == m_messages.cend()) {
        return talcs::NoteSynthesizerDetectorMessage::Null;
    } else {
        return *(m_messageIterator++);
    }
}
