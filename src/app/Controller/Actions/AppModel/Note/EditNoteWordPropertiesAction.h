#ifndef EDITNOTESWORDPROPERTIESACTION_H
#define EDITNOTESWORDPROPERTIESACTION_H

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/History/IAction.h>
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

class SingingClip;

struct WordPropertyEditOptions {
    bool replacePronunciation = false;
    bool replacePronCandidates = false;
};

class EditNoteWordPropertiesAction final : public IAction {
public:
    /// Applies the shared word-property cascade against the clip's current
    /// fallback language.
    explicit EditNoteWordPropertiesAction(const QList<Note *> &notes,
                                          const QList<Note::WordProperties> &args,
                                          SingingClip *clip,
                                          const QList<WordPropertyEditOptions> &options = {});

    /// Applies already cascaded properties verbatim.
    ///
    /// Used when the voice source changes in the same undo step: the caller has
    /// to resolve the cascade against the target language context, so this
    /// action must not re-derive it from the clip. `oldResolvedLanguages` and
    /// `newResolvedLanguages` describe each note before and after the change.
    explicit EditNoteWordPropertiesAction(const QList<Note *> &notes,
                                          const QList<Note::WordProperties> &applied,
                                          const QList<QString> &oldResolvedLanguages,
                                          const QList<QString> &newResolvedLanguages,
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
