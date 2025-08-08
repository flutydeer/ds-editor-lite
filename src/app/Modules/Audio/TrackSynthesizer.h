#ifndef TRACKSYNTHESIZER_H
#define TRACKSYNTHESIZER_H

#include <QObject>
#include <QMutex>
#include <QHash>

#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsDspx/DspxPseudoSingerContext.h>

namespace talcs {
    class DspxTrackContext;
    class DspxNoteContext;
}

class Track;
class SingingClip;
class Note;

class TrackSynthesizer : public talcs::DspxPseudoSingerContext {
    Q_OBJECT
public:
    explicit TrackSynthesizer(talcs::DspxTrackContext *trackContext, Track *track);
    ~TrackSynthesizer() override;

private:
    Track *m_track;

    QHash<SingingClip *, talcs::DspxSingingClipContext *> m_singingClipModelDict;
    QHash<Note *, talcs::DspxNoteContext *> m_noteModelDict;

    void handleSingingClipInserted(SingingClip *clip);
    void handleSingingClipRemoved(SingingClip *clip);

    void handleSingingClipPropertyChanged(SingingClip *clip) const;
    void handleNoteInserted(SingingClip *clip, Note *note);
    void handleNoteRemoved(SingingClip *clip, Note *note);

    void handleNotePropertyChanged(Note *note) const;

    void handleTimeChanged() const;
};

#endif // TRACKSYNTHESIZER_H
