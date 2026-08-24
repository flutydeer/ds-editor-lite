#ifndef INFERENCEAUTOMATIONBRIDGE_H
#define INFERENCEAUTOMATIONBRIDGE_H

#include "Automation/InferenceAutomationFacade.h"

namespace InferenceAutomationBridge {

    [[nodiscard]] Automation::DocumentVersion currentDocumentVersion();
    Automation::AutomationResult<Automation::InferenceMutationResultDto>
        executeAfterGate(const Automation::DocumentVersion &taskVersion,
                         const Automation::InferenceMutationRequest &request);
    Automation::AutomationResult<Automation::InferenceMutationResultDto>
        executeCurrent(const Automation::InferenceMutationRequest &request);
    [[nodiscard]] QString dropReason(const Automation::AutomationError &error);

} // namespace InferenceAutomationBridge

#endif // INFERENCEAUTOMATIONBRIDGE_H
