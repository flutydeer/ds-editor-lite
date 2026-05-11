//
// Created by fluty on 26-5-8.
//

#include "DeveloperOption.h"

void DeveloperOption::load(const QJsonObject &object) {
    if (object.contains(enableDiagnosticsKey))
        enableDiagnostics = object[enableDiagnosticsKey].toBool();
    if (object.contains(showTimelineDebugInfoKey))
        showTimelineDebugInfo = object[showTimelineDebugInfoKey].toBool();
    if (object.contains(showClipDebugInfoKey))
        showClipDebugInfo = object[showClipDebugInfoKey].toBool();
}

void DeveloperOption::save(QJsonObject &object) {
    object = {
        serialize_enableDiagnostics(),
        serialize_showTimelineDebugInfo(),
        serialize_showClipDebugInfo(),
    };
}
