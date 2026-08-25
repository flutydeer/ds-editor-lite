#ifndef ADDTRACKSACTION_H
#define ADDTRACKSACTION_H

#include <lite/History/IAction.h>

#include <QtTypes>
#include <memory>

class Track;
class AppModel;

class InsertTrackAction final : public IAction {
public:
    static InsertTrackAction *build(Track *track, qsizetype index, AppModel *model,
                                    bool resolveColorIndex = true);
    ~InsertTrackAction() override;
    void execute() override;
    void undo() override;

private:
    Track *m_track = nullptr;
    std::unique_ptr<Track> m_ownedTrack;
    qsizetype m_index = -1;
    AppModel *m_model = nullptr;
    bool m_resolveColorIndex = true;
};



#endif // ADDTRACKSACTION_H
