//
// Created by fluty on 2024/7/17.
//

#ifndef VALIDATIONCONTROLLER_H
#define VALIDATIONCONTROLLER_H

#define validationController ValidationController::instance()

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"
#include "Model/AppStatus/AppStatus.h"
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
    void onNoteChanged(SingingClip::NoteChangeType type, Note *note);
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);

signals:
    void validationFinished(bool passed);

private:
    void handleClipInserted(Clip *clip);
    void handleNoteInserted(Note *note);
    void handleNotePropertyChanged(Note::NotePropertyType type, Note *note);
    void validate();
    static bool validateProjectLength();
    static bool validateTempo();
    static bool validateClipOverlap();
    static bool validateNoteOverlap();
    void processPendingUpdateNotes();

    QList<Track *> m_tracks;
    QList<Clip *> m_clips;

    QList<Note *> m_notesPendingUpdateNoteWordProperty;
};



#endif // VALIDATIONCONTROLLER_H
