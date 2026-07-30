#include "EditorCanvasFactory.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Views/ClipEditor/PianoRoll/LegacyPianoRollCanvasAdapter.h"
#include "UI/Views/ClipEditor/PianoRoll/RhiPianoRollCanvas.h"
#include "UI/Views/TrackEditor/LegacyTrackCanvasAdapter.h"
#include "UI/Views/TrackEditor/RhiTrackCanvas.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcEditorCanvasFactory, "ds.editor.canvas.factory")

EditorCanvasBackend EditorCanvasFactory::startupBackend() {
    static const auto backend = [] {
        const auto selected = appOptions->developer()->editorCanvasBackend();
        qCInfo(lcEditorCanvasFactory)
            << "Editor canvas startup backend:" << editorCanvasBackendKey(selected);
        return selected;
    }();
    return backend;
}

ITrackEditorCanvas *EditorCanvasFactory::createTrackCanvas(QObject *parent) {
    return createTrackCanvas(startupBackend(), parent);
}

ITrackEditorCanvas *EditorCanvasFactory::createTrackCanvas(const EditorCanvasBackend backend,
                                                           QObject *parent) {
    ITrackEditorCanvas *canvas = nullptr;
    switch (backend) {
        case EditorCanvasBackend::ExperimentalRhi:
            canvas = new RhiTrackCanvas(parent);
            break;
        case EditorCanvasBackend::Legacy:
        default:
            canvas = new LegacyTrackCanvasAdapter(parent);
            break;
    }
    const auto backendKey = editorCanvasBackendKey(canvas->backend());
    qCInfo(lcEditorCanvasFactory) << "Created TrackEditor canvas" << backendKey << canvas;
    QObject::connect(canvas, &QObject::destroyed, [backendKey] {
        qCInfo(lcEditorCanvasFactory) << "Destroyed TrackEditor canvas" << backendKey;
    });
    return canvas;
}

IPianoRollCanvas *EditorCanvasFactory::createPianoRollCanvas(QObject *parent) {
    return createPianoRollCanvas(startupBackend(), parent);
}

IPianoRollCanvas *EditorCanvasFactory::createPianoRollCanvas(const EditorCanvasBackend backend,
                                                             QObject *parent) {
    IPianoRollCanvas *canvas = nullptr;
    switch (backend) {
        case EditorCanvasBackend::ExperimentalRhi:
            canvas = new RhiPianoRollCanvas(parent);
            break;
        case EditorCanvasBackend::Legacy:
        default:
            canvas = new LegacyPianoRollCanvasAdapter(parent);
            break;
    }
    const auto backendKey = editorCanvasBackendKey(canvas->backend());
    qCInfo(lcEditorCanvasFactory) << "Created PianoRoll canvas" << backendKey << canvas;
    QObject::connect(canvas, &QObject::destroyed, [backendKey] {
        qCInfo(lcEditorCanvasFactory) << "Destroyed PianoRoll canvas" << backendKey;
    });
    return canvas;
}
