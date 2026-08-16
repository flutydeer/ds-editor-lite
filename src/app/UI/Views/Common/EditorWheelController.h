#ifndef EDITORWHEELCONTROLLER_H
#define EDITORWHEELCONTROLLER_H

#include <lite/GUI/Controls/WheelInputController.h>

class EditorViewportController;
class QNativeGestureEvent;
class QWidget;
class QWheelEvent;

[[nodiscard]] bool isEditorWheelAnimationEnabled();

class EditorWheelController final {
public:
    explicit EditorWheelController(EditorViewportController *viewport, QWidget *widget);

    bool handleWheel(QWheelEvent *event);
    bool handleNativeGesture(QNativeGestureEvent *event);
    bool horizontalScale(QWheelEvent *event);
    bool verticalScale(QWheelEvent *event);
    bool horizontalScroll(QWheelEvent *event);
    bool verticalScroll(QWheelEvent *event);
    void stop();

private:
    EditorViewportController *m_viewport;
    QWidget *m_widget;
    WheelInputController m_input;
};

#endif // EDITORWHEELCONTROLLER_H
