#ifndef IDOCUMENTWORKFLOWUI_H
#define IDOCUMENTWORKFLOWUI_H

#include "ProjectLoadTypes.h"

#include <QString>

class QWidget;

enum class SaveDecision { Save, Discard, Cancel };

class IDocumentWorkflowUi {
public:
    virtual ~IDocumentWorkflowUi() = default;

    [[nodiscard]] virtual QWidget *documentWorkflowParentWidget() = 0;
    [[nodiscard]] virtual SaveDecision askDocumentSaveDecision() = 0;
    [[nodiscard]] virtual QString chooseDocumentSavePath(const QString &suggestedPath) = 0;
    [[nodiscard]] virtual bool confirmOpenWithoutPackageMetadata() = 0;
    virtual void showDocumentWorkflowError(const ProjectOperationError &error) = 0;
    virtual void showDocumentWorkflowBusy() = 0;
};

#endif // IDOCUMENTWORKFLOWUI_H
