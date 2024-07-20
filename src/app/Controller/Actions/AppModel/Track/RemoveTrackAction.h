//
// Created by fluty on 2024/2/8.
//

#ifndef REMOVETRACKSACTION_H
#define REMOVETRACKSACTION_H

#include "Modules/History/IAction.h"

#include <QList>

class Track;
class AppModel;

class RemoveTrackAction : public IAction {
public:
    static RemoveTrackAction *build(Track *track, AppModel *model);
    ~RemoveTrackAction() override;
    void execute() override;
    void undo() override;

private:
    Track *m_track = nullptr;
    AppModel *m_model = nullptr;
    QList<Track *> m_originalTracks;
};

#endif //REMOVETRACKSACTION_H
