#ifndef BATCHIMPORTACTIONS_H
#define BATCHIMPORTACTIONS_H

#include <lite/History/ActionSequence.h>
#include <lite/MusicBase/Tempo.h>
#include <lite/MusicBase/TimeSignature.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QList>

class AppModel;

// One undoable history item that commits a whole prepared import batch:
// optional tempo/time-signature changes first, then per-item clip insertion
// into existing tracks or into tracks created on the fly.
class BatchImportActions final : public ActionSequence {
public:
    // One committed unit: the clip to insert plus, when a new track was
    // allocated, the track that carries the clip. Exactly one of
    // `existingTrack` / `newTrack` is set.
    struct Item {
        Clip *clip = nullptr;
        Track *existingTrack = nullptr;
        Track *newTrack = nullptr;
    };

    static BatchImportActions *build(const QList<Tempo> &oldTempos, const QList<Tempo> &newTempos,
                                     const QList<TimeSignature> &oldSignatures,
                                     const QList<TimeSignature> &newSignatures,
                                     const QList<Item> &items, AppModel *model);
};

#endif // BATCHIMPORTACTIONS_H
