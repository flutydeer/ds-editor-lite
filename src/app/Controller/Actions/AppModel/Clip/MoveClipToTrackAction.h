#ifndef MOVECLIPTOTRACKACTION_H
#define MOVECLIPTOTRACKACTION_H

#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/History/IAction.h>
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

class Track;

class MoveClipToTrackAction : public IAction {
public:
    static MoveClipToTrackAction *build(const Clip::ClipCommonProperties &oldArgs,
                                        const Clip::ClipCommonProperties &newArgs, Clip *clip,
                                        Track *oldTrack, Track *newTrack);
    void execute() override;
    void undo() override;

private:
    void applyWordProperties(const QList<Note::WordProperties> &properties);

    Clip::ClipCommonProperties m_oldArgs;
    Clip::ClipCommonProperties m_newArgs;
    QList<SingingClipPhonemeNormalizer::ResetRecord> m_resetRecords;
    // Notes whose resolved language follows the track singer, with the word
    // data before and after the move. Moving is one undo step, so the cascade
    // travels with it.
    QList<Note *> m_cascadedNotes;
    QList<Note::WordProperties> m_oldWordProperties;
    QList<Note::WordProperties> m_newWordProperties;
    Clip *m_clip = nullptr;
    Track *m_oldTrack = nullptr;
    Track *m_newTrack = nullptr;
    bool m_audioTimingChanged = false;
};

#endif // MOVECLIPTOTRACKACTION_H
