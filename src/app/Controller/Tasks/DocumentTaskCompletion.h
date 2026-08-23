#ifndef DOCUMENTTASKCOMPLETION_H
#define DOCUMENTTASKCOMPLETION_H

#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"

#include <QObject>

#include <utility>

namespace DocumentTaskCompletion {
    template <typename TaskType, typename Resume>
    bool deferCompletionWhileDocumentBusy(Automation::CoreRuntime *runtime,
                                          DocumentWorkflowController *workflow, TaskType *task,
                                          QObject *receiver, Resume resume) {
        if (!runtime || !workflow ||
            runtime->documentVersion().documentId != task->documentVersion.documentId ||
            (!runtime->documentBusy(task->documentVersion.documentId) && !workflow->busy()))
            return false;
        QObject::connect(
            workflow, &DocumentWorkflowController::busyChanged, receiver,
            [task, resume = std::move(resume)](const bool busy) mutable {
                if (!busy)
                    resume(task);
            },
            Qt::SingleShotConnection);
        return true;
    }
}

#endif // DOCUMENTTASKCOMPLETION_H
