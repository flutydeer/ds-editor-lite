#ifndef PARAMEDITOREDITMODE_H
#define PARAMEDITOREDITMODE_H

#include "Interface/EditorViewState.h"

using ParamEditorEditMode = EditorViewGlobal::ParameterEditMode;

[[nodiscard]] inline bool isParamEditorEditModeVisible(const ParamEditorEditMode mode,
                                                       const ParamInfo::Name parameter) {
    switch (mode) {
        case ParamEditorEditMode::Draw:
        case ParamEditorEditMode::Erase:
        case ParamEditorEditMode::Anchor:
            return true;
        case ParamEditorEditMode::Bake:
        case ParamEditorEditMode::Shape:
        case ParamEditorEditMode::Scale:
            return ParamInfo::hasOriginalParam(parameter);
    }
    return false;
}

#endif // PARAMEDITOREDITMODE_H
