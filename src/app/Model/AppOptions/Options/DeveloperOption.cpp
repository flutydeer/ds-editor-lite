//
// Created by fluty on 26-5-8.
//

#include "DeveloperOption.h"

namespace {
    constexpr auto kEditorRenderBackendKey = "editorRenderBackend";
}

void DeveloperOption::load(const QJsonObject &object) {
    load_enableDiagnostics(object);
    load_showLogWindow(object);
    load_showTimelineDebugInfo(object);
    load_showClipDebugInfo(object);
    load_enablePanelDetach(object);
    editorRenderBackend = editorRenderBackendFromString(
        object.value(QLatin1String(kEditorRenderBackendKey)).toString());
}

void DeveloperOption::save(QJsonObject &object) {
    object = {
        serialize_enableDiagnostics(),
        serialize_showLogWindow(),
        serialize_showTimelineDebugInfo(),
        serialize_showClipDebugInfo(),
        serialize_enablePanelDetach(),
        {QLatin1String(kEditorRenderBackendKey), editorRenderBackendToString(editorRenderBackend)},
    };
}

QString DeveloperOption::editorRenderBackendToString(const EditorRenderBackend backend) {
    switch (backend) {
        case EditorRenderBackend::RhiExperimental:
            return QStringLiteral("rhi-experimental");
        case EditorRenderBackend::Legacy:
        default:
            return QStringLiteral("legacy");
    }
}

DeveloperOption::EditorRenderBackend DeveloperOption::editorRenderBackendFromString(
    const QString &value) {
    if (value.compare(QStringLiteral("rhi-experimental"), Qt::CaseInsensitive) == 0)
        return EditorRenderBackend::RhiExperimental;
    return EditorRenderBackend::Legacy;
}
