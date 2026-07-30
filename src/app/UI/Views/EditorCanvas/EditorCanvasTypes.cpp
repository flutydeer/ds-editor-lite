#include "EditorCanvasTypes.h"

QString editorCanvasBackendKey(const EditorCanvasBackend backend) {
    switch (backend) {
        case EditorCanvasBackend::ExperimentalRhi:
            return QStringLiteral("experimental-rhi");
        case EditorCanvasBackend::Legacy:
        default:
            return QStringLiteral("legacy");
    }
}

EditorCanvasBackend editorCanvasBackendFromKey(const QString &key) {
    if (key == QStringLiteral("experimental-rhi"))
        return EditorCanvasBackend::ExperimentalRhi;
    return EditorCanvasBackend::Legacy;
}
