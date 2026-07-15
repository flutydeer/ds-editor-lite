//
// Created by fluty on 2024/2/8.
//

#ifndef REMOVETRACKSACTION_H
#define REMOVETRACKSACTION_H

#include "Modules/History/IAction.h"

#include <memory>
#include <QtTypes>

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
    std::unique_ptr<Track> m_ownedTrack;
    AppModel *m_model = nullptr;
    qsizetype m_index = -1;
};

#endif // REMOVETRACKSACTION_H
