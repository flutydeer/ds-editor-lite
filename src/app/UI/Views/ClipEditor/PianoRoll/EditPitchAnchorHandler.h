#ifndef EDITPITCHANCHORHANDLER_H
#define EDITPITCHANCHORHANDLER_H

#include "PianoRollEditHandler.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditController.h"

class AnchorCurve;
class QContextMenuEvent;
struct PianoRollMenuContext;
enum class PianoRollAnchorMode;

class EditPitchAnchorHandler final : public PianoRollEditHandler {
public:
    EditPitchAnchorHandler();

    void activate() override;
    void deactivate() override;
    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    bool keyPressEvent(QKeyEvent *event) override;
    void commit() override;
    void discard() override;
    [[nodiscard]] Qt::Orientations edgeAutoScrollAxes() const override;
    void continueDragAt(const QPoint &viewportPos) override;

    void setAlwaysVisible(bool visible);
    void loadFromModel(const QList<AnchorCurve *> &curves);
    [[nodiscard]] const AnchorEditor::AnchorOverlayState &overlayState() const;
    bool prepareMenuContext(QContextMenuEvent *event, PianoRollMenuContext &context);
    void setSelectedInterpolation(PianoRollAnchorMode mode);
    void deleteSelectedNodesFromMenu();

private:
    bool beginEditSession();
    void publish(const QList<AnchorCurve *> &curves);
    void finishEditSession(AnchorEditor::EditFinishReason reason);
    void triggerRepaint() const;

    AnchorEditor::AnchorEditController m_controller;
    quint64 m_sessionId = 0;
};

#endif // EDITPITCHANCHORHANDLER_H
