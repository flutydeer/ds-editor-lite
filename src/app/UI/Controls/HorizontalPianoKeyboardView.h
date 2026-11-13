#ifndef HORIZONTALPIANOKEYBOARDVIEW_H
#define HORIZONTALPIANOKEYBOARDVIEW_H

#include "UI/Controls/PianoKeyboardBase.h"

#include <QList>

/// Horizontal keyboard strip for auditioning the synthesizer without a MIDI
/// keyboard. It spans the C2-C6 range (five octaves ending on the closing C),
/// lays the keys out the way a real piano does, and reports presses through
/// signals so the owner decides where the notes go.
class HorizontalPianoKeyboardView : public PianoKeyboardBase {
    Q_OBJECT

public:
    explicit HorizontalPianoKeyboardView(QWidget *parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void noteOn(int key);
    void noteOff(int key);

protected:
    int keyAtPosition(const QPointF &pos) const override;
    void paintKeyboard(QPainter &painter) const override;
    void playNoteOn(int key) override;
    void playNoteOff(int key) override;
    void keyRangeChanged() override;

private:
    void rebuildLayout();
    [[nodiscard]] int whiteKeyCount() const;
    [[nodiscard]] double totalWidth() const;
    [[nodiscard]] int blackKeyAtBoundary(int boundary) const;
    [[nodiscard]] QRectF whiteKeyRect(int whiteIndex) const;
    [[nodiscard]] QRectF blackKeyRect(int boundary) const;
    [[nodiscard]] int keyTintAlpha(int key) const;

    // MIDI numbers of the white keys, low to high.
    QList<int> m_whiteKeys;
    // Black key centred on each white-key boundary; -1 when the boundary has no
    // black key (the bed's outer edges included).
    QList<int> m_blackKeysByBoundary;
};

#endif // HORIZONTALPIANOKEYBOARDVIEW_H
