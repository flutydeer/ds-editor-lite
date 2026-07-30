//
// Created by fluty on 26-5-8.
//

#include "DeveloperOption.h"

void DeveloperOption::load(const QJsonObject &object) {
    load_enableDiagnostics(object);
    load_showLogWindow(object);
    load_showTimelineDebugInfo(object);
    load_showClipDebugInfo(object);
    load_enablePanelDetach(object);
    load_editorRenderer(object);
    editorRenderer = editorCanvasBackendKey(editorCanvasBackendFromKey(editorRenderer));
}

void DeveloperOption::save(QJsonObject &object) {
    object = {
        serialize_enableDiagnostics(),     serialize_showLogWindow(),
        serialize_showTimelineDebugInfo(), serialize_showClipDebugInfo(),
        serialize_enablePanelDetach(),     serialize_editorRenderer(),
    };
}

EditorCanvasBackend DeveloperOption::editorCanvasBackend() const {
    return editorCanvasBackendFromKey(editorRenderer);
}

void DeveloperOption::setEditorCanvasBackend(const EditorCanvasBackend backend) {
    editorRenderer = editorCanvasBackendKey(backend);
}
