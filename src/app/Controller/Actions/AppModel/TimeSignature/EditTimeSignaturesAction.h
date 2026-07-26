#ifndef EDITTIMESIGNATURESACTION_H
#define EDITTIMESIGNATURESACTION_H

#include "Modules/History/IAction.h"

#include <lite/MusicBase/TimeSignature.h>

#include <QList>

class AppModel;

// Replaces the whole time signature sequence of the project timeline. Used by
// project import when the source contains signature changes.
class EditTimeSignaturesAction final : public IAction {
public:
    static EditTimeSignaturesAction *build(const QList<TimeSignature> &oldSignatures,
                                           const QList<TimeSignature> &newSignatures,
                                           AppModel *model);
    void execute() override;
    void undo() override;

private:
    QList<TimeSignature> m_oldSignatures;
    QList<TimeSignature> m_newSignatures;
    AppModel *m_model = nullptr;
};

#endif // EDITTIMESIGNATURESACTION_H
