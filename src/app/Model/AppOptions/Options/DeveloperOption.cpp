//
// Created by fluty on 26-5-8.
//

#include "DeveloperOption.h"

void DeveloperOption::load(const QJsonObject &object) {
    load_enableDiagnostics(object);
    load_showTimelineDebugInfo(object);
    load_showClipDebugInfo(object);
    load_enablePanelDetach(object);
}

void DeveloperOption::save(QJsonObject &object) {
    object = {
        serialize_enableDiagnostics(),
        serialize_showTimelineDebugInfo(),
        serialize_showClipDebugInfo(),
        serialize_enablePanelDetach(),
    };
}
