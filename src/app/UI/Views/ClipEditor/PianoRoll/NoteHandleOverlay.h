#ifndef NOTEHANDLEOVERLAY_H
#define NOTEHANDLEOVERLAY_H

#include <lite/GUI/Base/IScalableItem.h>

#include <QColor>
#include <QGraphicsItem>
#include <QRectF>

// The resize handle ring on the selected note (Legacy backend).
//
// The whole ring band lies outside the note (the hole's four edges are the note's
// visible edges), so painting it inside NoteView::paint would leave artifacts by
// exceeding the boundingRect — and NoteView's boundingRect is reused by lyric
// layout and the inline editor, so it cannot be changed for the ring's sake. A
// high-z overlay layer also solves neighbouring notes covering each other along
// the way.
//
// Holds only the note's model rectangle (scene coordinates), never a NoteView
// pointer, so deleting a note leaves no dangling reference.
class NoteHandleOverlay final : public QGraphicsItem, public IScalableItem {
public:
    NoteHandleOverlay();

    // The target note's model rectangle (scene coordinates). Empty means draw nothing.
    void setNoteSceneRect(const QRectF &sceneRect);
    [[nodiscard]] QRectF noteSceneRect() const;

    // Hit testing: whether this scene point falls on the ring's left/right bands
    // (the top/bottom segments are decoration only)
    [[nodiscard]] bool frameContains(const QPointF &scenePos) const;

    [[nodiscard]] QColor fillColor() const;
    void setFillColor(const QColor &color);
    [[nodiscard]] QColor borderColor() const;
    void setBorderColor(const QColor &color);
    [[nodiscard]] QColor gripColor() const;
    void setGripColor(const QColor &color);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

protected:
    void afterSetScale() override;
    void afterSetVisibleRect() override;

private:
    QRectF m_noteRect;
    QColor m_fillColor{255, 255, 255};
    QColor m_borderColor{31, 0, 0, 0};
    QColor m_gripColor{64, 0, 0, 0};
};

#endif // NOTEHANDLEOVERLAY_H
