#ifndef GHOSTNOTESOURCE_H
#define GHOSTNOTESOURCE_H

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRectF>

class SingingClip;

// A reference note from another track. Time is expressed in absolute ticks, so each
// rendering backend subtracts the current clip's start() to reach scene coordinates.
struct GhostNote {
    int globalStart = 0;
    int length = 0;
    int keyIndex = 60;
    int colorIndex = 0; // colorIndex of the owning track
};

// Visual parameters and colors shared by both rendering backends
namespace GhostNoteStyle {
    inline constexpr double heightRatio = 0.2; // fraction of the key row height
    inline constexpr double minHeight = 2.0;   // lower bound in logical pixels
    inline constexpr double opacity = 0.45;    // alpha factor applied to noteBackground

    QColor fillColor(int colorIndex);

    // Turns the bar's left/right bounds into the rect actually painted. The horizontal
    // inset matches half a regular note's border width, so two back-to-back notes on the
    // same key keep the same gap regular notes do instead of merging into one bar.
    QRectF barRect(double left, double right, double top, double height);
}

// Collects the notes of every singing clip outside the host track and refreshes them when
// the model or the appearance option changes. The legacy backend (GhostNoteOverlay) and the
// RHI one (PianoRollRhiWidget) each own an instance.
class GhostNoteSource final : public QObject {
    Q_OBJECT

public:
    explicit GhostNoteSource(QObject *parent = nullptr);

    // The clip the piano roll is editing; nullptr clears the list
    void setHostClip(SingingClip *clip);
    // True when the option is on and a host clip is set
    [[nodiscard]] bool enabled() const;
    // Sorted by globalStart, ascending
    [[nodiscard]] const QList<GhostNote> &notes() const;
    // Longest note in the list, used to binary search the start of the visible range
    [[nodiscard]] int maxLength() const;

signals:
    void changed();

private:
    void scheduleRebuild();
    void rebuild();
    void rebuildConnections();
    void clearConnections();

    QPointer<SingingClip> m_hostClip;
    QList<GhostNote> m_notes;
    int m_maxLength = 0;
    bool m_enabled = false;
    bool m_rebuildScheduled = false;
    bool m_connectionsBuilt = false;
};

#endif // GHOSTNOTESOURCE_H
