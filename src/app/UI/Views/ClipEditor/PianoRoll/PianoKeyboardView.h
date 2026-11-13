#ifndef PIANOKEYBOARDVIEW_H
#define PIANOKEYBOARDVIEW_H

#include "UI/Controls/PianoKeyboardBase.h"

/// Vertical key column of the piano roll: the key bed follows the canvas key
/// range, and clicking or dragging a key plays notes through the MIDI system.
class PianoKeyboardView : public PianoKeyboardBase {
    Q_OBJECT

public:
    explicit PianoKeyboardView(QWidget *parent = nullptr);

protected:
    int keyAtPosition(const QPointF &pos) const override;
    void paintKeyboard(QPainter &painter) const override;
    void playNoteOn(int key) override;
    void playNoteOff(int key) override;

private:
    [[nodiscard]] double pixelsPerBlackKey() const;
    [[nodiscard]] double pixelsPerWhiteKey() const;
    [[nodiscard]] double blackKeyY(double key) const;
    [[nodiscard]] double closestBlackKeyY(double key) const;
    [[nodiscard]] double whiteKeyY(double key) const;
    void drawWhiteKeyOverlay(QPainter &painter, int keyIndex, int alpha) const;
    void drawBlackKeyOverlay(QPainter &painter, int keyIndex, int alpha) const;
};

#endif // PIANOKEYBOARDVIEW_H
