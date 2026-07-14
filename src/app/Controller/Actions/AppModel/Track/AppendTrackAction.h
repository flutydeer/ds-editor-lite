//
// Created by fluty on 2024/2/8.
//

#ifndef APPENDTRACKSACTION_H
#define APPENDTRACKSACTION_H

#include "Modules/History/IAction.h"

#include <memory>

class Track;
class AppModel;

class AppendTrackAction final : public IAction {
public:
    static AppendTrackAction *build(Track *track, AppModel *model);
    ~AppendTrackAction() override;
    void execute() override;
    void undo() override;

private:
    Track *m_track = nullptr;
    std::unique_ptr<Track> m_ownedTrack;
    AppModel *m_model = nullptr;
};



#endif // APPENDTRACKSACTION_H
