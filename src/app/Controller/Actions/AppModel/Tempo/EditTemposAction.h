#ifndef EDITTEMPOSACTION_H
#define EDITTEMPOSACTION_H

#include <lite/History/IAction.h>
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

#include <lite/MusicBase/Tempo.h>

#include <QList>

class AppModel;
class SingingClip;

// Replaces the whole tempo sequence of the project timeline. Used by project
// import when the source contains more than a single tempo point.
class EditTemposAction final : public IAction {
public:
    static EditTemposAction *build(const QList<Tempo> &oldTempos, const QList<Tempo> &newTempos,
                                   AppModel *model);
    void execute() override;
    void undo() override;

private:
    class ClipResetRecords {
    public:
        SingingClip *clip = nullptr;
        QList<SingingClipPhonemeNormalizer::ResetRecord> records;
    };

    QList<Tempo> m_oldTempos;
    QList<Tempo> m_newTempos;
    QList<ClipResetRecords> m_resetRecords;
    AppModel *m_model = nullptr;
};

#endif // EDITTEMPOSACTION_H
