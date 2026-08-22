#include "InferenceAutomationBridge.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"

namespace InferenceAutomationBridge {
    namespace {
        Automation::AutomationError unavailable() {
            Automation::AutomationError error;
            error.code = Automation::AutomationErrorCode::ModuleNotReady;
            error.message = QStringLiteral("Automation runtime is unavailable");
            return error;
        }

        Automation::AutomationResult<Automation::InferenceMutationResultDto>
        execute(const Automation::DocumentVersion &expected,
                const Automation::InferenceMutationRequest &request) {
            auto *runtime = AppContext::instance<Automation::CoreRuntime>();
            if (!runtime)
                return unavailable();
            Automation::CommandContext context;
            context.expected = expected;
            context.source = Automation::InvocationSource::InternalAutomation;
            return runtime->inference().applyMutation(context, request);
        }
    }

    Automation::DocumentVersion currentDocumentVersion() {
        const auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        return runtime ? runtime->documentVersion() : Automation::DocumentVersion{};
    }

    Automation::AutomationResult<Automation::InferenceMutationResultDto>
    executeAfterGate(const Automation::DocumentVersion &taskVersion,
                     const Automation::InferenceMutationRequest &request) {
        const auto commitVersion = Automation::rebaseValidatedInferenceTaskVersion(
            taskVersion, currentDocumentVersion());
        if (!commitVersion)
            return commitVersion.getError();
        return execute(commitVersion.get(), request);
    }

    Automation::AutomationResult<Automation::InferenceMutationResultDto>
    executeCurrent(const Automation::InferenceMutationRequest &request) {
        return execute(currentDocumentVersion(), request);
    }

    QString dropReason(const Automation::AutomationError &error) {
        switch (error.code) {
            case Automation::AutomationErrorCode::DocumentChanged:
                return QStringLiteral("document-changed");
            case Automation::AutomationErrorCode::RevisionConflict:
                return QStringLiteral("document-revision-mismatch");
            case Automation::AutomationErrorCode::NotFound:
                return QStringLiteral("object-not-found");
            case Automation::AutomationErrorCode::WrongObjectType:
                return QStringLiteral("object-type-mismatch");
            default:
                return QStringLiteral("automation-apply-failed");
        }
    }

} // namespace InferenceAutomationBridge
