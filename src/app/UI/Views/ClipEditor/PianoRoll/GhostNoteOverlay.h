#ifndef GHOSTNOTEOVERLAY_H
#define GHOSTNOTEOVERLAY_H

#include "GhostNoteSource.h"
#include "UI/Views/Common/TimeOverlayView.h"

// Paints the other tracks' notes as thin bars from a single viewport-following overlay
// rather than one item per note. A project can hold tens of thousands of notes on the other
// tracks, and one QGraphicsItem each would bog down the scene and its hit testing.
class GhostNoteOverlay final : public TimeOverlayView {
    Q_OBJECT

public:
    GhostNoteOverlay();

    // Only the pointer is held; PianoRollGraphicsViewPrivate owns the source's lifetime
    void setSource(const GhostNoteSource *source);
    // The current clip's start(), which is the scene X origin
    void setOffset(int offset);

protected:
    void updateRectAndPos() override;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    const GhostNoteSource *m_source = nullptr;
    int m_offset = 0;
};

#endif // GHOSTNOTEOVERLAY_H
