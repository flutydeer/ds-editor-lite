#ifndef MODELCHANGEHANDLER_H
#define MODELCHANGEHANDLER_H

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>


#include <QObject>

class ModelChangeHandler : public QObject {
    Q_OBJECT

protected:
    explicit ModelChangeHandler(QObject *parent = nullptr);

    [[nodiscard]] const QList<TempoChangeRange> &tempoChangeRanges() const;

    virtual void handleModelChanged();
    // Fired only when the tempo side of the timeline actually changed;
    // signature-only edits do not reach tempo-sensitive handlers.
    virtual void handleTempoChanged();
    virtual void handleTrackInserted(Track *track);
    virtual void handleTrackRemoved(Track *track);
    virtual void handleClipInserted(Clip *clip);
    virtual void handleClipRemoved(Clip *clip);
    virtual void handleClipPropertyChanged(Clip *clip);
    virtual void handleSingingClipInserted(SingingClip *clip);
    virtual void handleSingingClipRemoved(SingingClip *clip);
    virtual void handleNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes,
                                   SingingClip *clip);
    virtual void handleParamChanged(ParamInfo::Name name, Param::Type type, SingingClip *clip);
    virtual void handleVoiceContextChanged(const VoiceContextChange &change, SingingClip *clip);
    virtual void handlePiecesChanged(const PieceList &pieces, const PieceList &discardedPieces,
                                     SingingClip *clip);

private slots:
    void onModelChanged();
    void onTimelineChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);

private:
    QList<Track *> m_tracks;
    Timeline m_timelineSnapshot;
    QList<TempoChangeRange> m_tempoChangeRanges;
};



#endif // MODELCHANGEHANDLER_H
