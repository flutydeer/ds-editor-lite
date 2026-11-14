#ifndef PIANOKEYBOARDBASE_H
#define PIANOKEYBOARDBASE_H

#include <QEnterEvent>
#include <QMouseEvent>
#include <QSet>
#include <QWidget>

class QPainter;

/// Shared core of the piano keyboards: palette and QSS colour properties, key
/// range, hover/pressed state, pointer input with glissando, and the wheel
/// forwarding contract. Subclasses own the geometry, the painting and the note
/// output - the only parts that differ between the piano roll's vertical key
/// column and a horizontal preview strip.
class PianoKeyboardBase : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor whiteKeyColor READ whiteKeyColor WRITE setWhiteKeyColor)
    Q_PROPERTY(QColor blackKeyColor READ blackKeyColor WRITE setBlackKeyColor)
    Q_PROPERTY(QColor dividerColor READ dividerColor WRITE setDividerColor)

public:
    explicit PianoKeyboardBase(QWidget *parent = nullptr);

    QColor whiteKeyColor() const;
    void setWhiteKeyColor(const QColor &whiteKeyColor);
    QColor blackKeyColor() const;
    void setBlackKeyColor(const QColor &blackKeyColor);
    QColor dividerColor() const;
    void setDividerColor(const QColor &dividerColor);

    void setHoveredKeyIndex(int keyIndex);
    void setTrackColorIndex(int index);

    /// Marks a key as held by an external note source (an attached MIDI
    /// keyboard). Subclasses that paint the held state show it like a press.
    void setHeldKey(int key, bool held);
    void clearHeldKeys();
    [[nodiscard]] bool isKeyHeld(int key) const;

public slots:
    void setKeyRange(double top, double bottom);
    void refreshThemeColors();

signals:
    void wheelScroll(QWheelEvent *event);

protected:
    [[nodiscard]] int hoveredKeyIndex() const;
    [[nodiscard]] int pressedKeyIndex() const;
    [[nodiscard]] double keyRangeTop() const;
    [[nodiscard]] double keyRangeBottom() const;
    [[nodiscard]] const QSet<int> &heldKeys() const;
    /// Key highlight colour carrying \p alpha, for hover/pressed overlays.
    [[nodiscard]] QColor overlayColor(int alpha) const;

    /// Key under \p pos, or -1 when the position is outside the key bed.
    virtual int keyAtPosition(const QPointF &pos) const = 0;
    /// Paints the whole key bed, hover/pressed highlights included.
    virtual void paintKeyboard(QPainter &painter) const = 0;
    virtual void playNoteOn(int key) = 0;
    virtual void playNoteOff(int key) = 0;
    /// Called after the key range changed, so the subclass can rebuild the
    /// layout derived from it.
    virtual void keyRangeChanged();

    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void releaseCurrentNote();

    static constexpr qreal penWidth = 1;

    double m_top = 0;
    double m_bottom = 127;
    QColor m_whiteKeyColor = {218, 219, 224};
    QColor m_blackKeyColor = {59, 63, 71};
    QColor m_dividerColor = {170, 172, 181};
    QColor m_primaryColor;
    int m_trackColorIndex = 0;
    int m_hoveredKeyIndex = -1;
    int m_pressedKeyIndex = -1;
    QSet<int> m_heldKeys;
};

#endif // PIANOKEYBOARDBASE_H
