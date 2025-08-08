//
// Created by fluty on 2024/7/17.
//

#ifndef VALIDATIONCONTROLLER_H
#define VALIDATIONCONTROLLER_H

#define validationController ValidationController::instance()

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/Track.h"
#include "Utils/Singleton.h"

class Note;

class ValidationController : public QObject, public Singleton<ValidationController> {
    Q_OBJECT
public:
    explicit ValidationController();
    void runValidation();

private slots:
    void onUndoRedoChanged();
    void onModelChanged();
    void onTempoChanged(double tempo);
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);
    void onClipPropertyChanged(Clip *clip);
    void onNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);

signals:
    void validationFinished(bool passed);

private:
    void handleClipInserted(Clip *clip);
    void validate();
    static bool validateProjectLength();
    static bool validateTempo();
    static bool validateNoteOverlap();

    QList<Track *> m_tracks;
    QList<Clip *> m_clips;
};



#endif // VALIDATIONCONTROLLER_H
