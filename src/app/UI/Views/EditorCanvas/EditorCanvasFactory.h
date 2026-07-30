#ifndef EDITORCANVASFACTORY_H
#define EDITORCANVASFACTORY_H

#include "EditorCanvasTypes.h"

class IPianoRollCanvas;
class ITrackEditorCanvas;
class QObject;

class EditorCanvasFactory final {
public:
    [[nodiscard]] static EditorCanvasBackend startupBackend();

    [[nodiscard]] static ITrackEditorCanvas *createTrackCanvas(QObject *parent = nullptr);
    [[nodiscard]] static ITrackEditorCanvas *createTrackCanvas(EditorCanvasBackend backend,
                                                               QObject *parent = nullptr);

    [[nodiscard]] static IPianoRollCanvas *createPianoRollCanvas(QObject *parent = nullptr);
    [[nodiscard]] static IPianoRollCanvas *createPianoRollCanvas(EditorCanvasBackend backend,
                                                                 QObject *parent = nullptr);

private:
    EditorCanvasFactory() = delete;
};

#endif // EDITORCANVASFACTORY_H
