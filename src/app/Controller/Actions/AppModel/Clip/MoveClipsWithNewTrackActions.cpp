#include "MoveClipsWithNewTrackActions.h"

#include "Controller/Actions/AppModel/Track/InsertTrackAction.h"

MoveClipsWithNewTrackActions *MoveClipsWithNewTrackActions::build(
    Track *track, const qsizetype trackIndex, AppModel *model, const bool resolveColorIndex,
    const QList<ClipPropertyEdit> &edits) {
    const auto a = new MoveClipsWithNewTrackActions;
    a->setTranslatableName("MoveClipsWithNewTrackActions",
                           QT_TRANSLATE_NOOP("MoveClipsWithNewTrackActions",
                                             "Move clip(s) to new track"));
    a->addAction(InsertTrackAction::build(track, trackIndex, model, resolveColorIndex));
    if (!edits.isEmpty()) {
        const auto clips = new ClipActions;
        clips->editClips(edits);
        // The composite owns the history entry, so the clip focus transition has
        // to move up here or undo would not reveal what it moved.
        if (const auto &transition = clips->focusTransition())
            a->setFocusTransition(*transition);
        a->addAction(clips);
    }
    return a;
}
