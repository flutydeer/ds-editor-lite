#ifndef TRACKSYNTHESIZER_H
#define TRACKSYNTHESIZER_H

#include <memory>

#include <QObject>
#include <QMutex>

#include <TalcsCore/PositionableAudioSource.h>
#include <TalcsCore/NoteSynthesizer.h>

class Track;
class SingingClip;
class Note;

class TrackSynthesizer : public QObject, public talcs::PositionableAudioSource, public talcs::NoteSynthesizerDetector {
    Q_OBJECT
public:
    explicit TrackSynthesizer(Track *track);
    ~TrackSynthesizer() override;
    bool open(qint64 bufferSize, double sampleRate) override;
    void close() override;
    qint64 length() const override;
    qint64 nextReadPosition() const override;
    void setNextReadPosition(qint64 pos) override;
    void detectInterval(qint64 intervalLength) override;
    talcs::NoteSynthesizerDetectorMessage nextMessage() override;

protected:
    qint64 processReading(const talcs::AudioSourceReadData &readData) override;

private:
    std::unique_ptr<talcs::NoteSynthesizer> m_synthesizer;
    Track *m_track;
    QList<talcs::NoteSynthesizerDetectorMessage> m_messages;
    QList<talcs::NoteSynthesizerDetectorMessage>::const_iterator m_messageIterator;

    Track *m_replicaTrack;
    QMutex m_mutex;

    Track *replicateTrack(Track *track, QObject *parent);
    SingingClip *replicateSingingClip(SingingClip *singingClip, Track *track);
    Note *replicateNote(Note *note, SingingClip *singingClip);
};



#endif // TRACKSYNTHESIZER_H
