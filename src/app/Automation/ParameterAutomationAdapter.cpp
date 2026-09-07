#include "ParameterAutomationAdapter.h"

#include "Model/Utils/ParamUtils.h"
#include "UI/Views/ClipEditor/CurveTransform/PitchCurveTransformContext.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

namespace Automation {
    ParameterRuntimeServices createParameterAutomationServices() {
        return {
            .prepareTransform =
                [](SingingClip *clip, const ParamInfo::Name name,
                   const CurveTransform::Kind kind) -> AutomationResult<CurveTransform::Config> {
                CurveTransform::Config config;
                config.kind = kind;
                config.properties = paramUtils->getPropertiesByName(name);
                config.tickToMilliseconds = [timeline = appModel->timeline(),
                                             start = clip->start()](const int localTick) {
                    return timeline.tickToMs(start + localTick);
                };
                if (kind == CurveTransform::Kind::ModulatePitch) {
                    auto pitch = std::make_shared<CurveTransform::PitchContext>();
                    pitch->rebuild(clip);
                    if (pitch->partitions().isEmpty()) {
                        return AutomationError{AutomationErrorCode::OperationUnavailable,
                                               QStringLiteral("Pitch modulation requires segmented "
                                                              "notes with valid phoneme timing")};
                    }
                    config.partitions = pitch->partitions();
                    config.pitchBaselineAtTick = [pitch](const int tick) {
                        return pitch->baselineAtTick(tick);
                    };
                }
                return config;
            },
        };
    }
}
