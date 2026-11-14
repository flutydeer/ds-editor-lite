#ifndef MOVECLIPSWITHNEWTRACKACTIONS_H
#define MOVECLIPSWITHNEWTRACKACTIONS_H

#include "Controller/Actions/AppModel/Clip/ClipActions.h"

#include <lite/History/ActionSequence.h>

#include <QList>

class AppModel;
class Track;

// One undoable history item that creates a track and moves the clips that
// landed on it in the same step. Undo runs the moves back first and removes the
// track last, so the clips never point at a track that is already gone.
class MoveClipsWithNewTrackActions final : public ActionSequence {
public:
    static MoveClipsWithNewTrackActions *build(Track *track, qsizetype trackIndex, AppModel *model,
                                               bool resolveColorIndex,
                                               const QList<ClipPropertyEdit> &edits);
};

#endif // MOVECLIPSWITHNEWTRACKACTIONS_H
