#ifndef EDITORVIEWPORTCONTROLLER_H
#define EDITORVIEWPORTCONTROLLER_H

#include "EditorCanvasTypes.h"

#include <QObject>

class EditorViewportController final : public QObject {
    Q_OBJECT

public:
    explicit EditorViewportController(QObject *parent = nullptr);

    [[nodiscard]] const EditorViewportState &state() const;
    void restore(const EditorViewportState &state);
    void setCenter(double tick, double secondaryValue);
    bool setScale(double horizontalScale, double verticalScale);
    void setVisibleRange(double startTick, double endTick, double topValue, double bottomValue);
    void setViewportMetrics(const QSize &size, double devicePixelRatio);
    void setPlaybackPositions(double playbackPosition, double lastPlaybackPosition);

signals:
    void stateChanged(const EditorViewportState &state, EditorDirtyDomains domains);

private:
    static bool validScale(double value);
    void notify(EditorDirtyDomains domains);

    EditorViewportState m_state;
};

#endif // EDITORVIEWPORTCONTROLLER_H
