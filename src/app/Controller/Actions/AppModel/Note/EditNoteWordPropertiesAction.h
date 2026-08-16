#ifndef EDITNOTESWORDPROPERTIESACTION_H
#define EDITNOTESWORDPROPERTIESACTION_H

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/History/IAction.h>
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

class SingingClip;

class EditNoteWordPropertiesAction final : public IAction {
public:
    explicit EditNoteWordPropertiesAction(const QList<Note *> &notes,
                                          const QList<Note::WordProperties> &args,
                                          SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    QList<Note *> m_notes;
    QList<Note::WordProperties> m_oldArgs;
    QList<Note::WordProperties> m_newArgs;
    QList<SingingClipPhonemeNormalizer::ResetRecord> m_resetRecords;
    SingingClip *m_clip = nullptr;
    bool m_pronunciationOnly = false;
};



#endif // EDITNOTESWORDPROPERTIESACTION_H
