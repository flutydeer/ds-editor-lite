#ifndef HISTORYFOCUS_H
#define HISTORYFOCUS_H

#include <QList>

enum class HistoryFocusKind {
    PianoRollNotes,
    TrackClips,
    Parameter,
    Mixer,
};

struct HistoryFocus {
    HistoryFocusKind kind = HistoryFocusKind::TrackClips;
    QList<int> objectIds;
    int containerId = -1;
    int trackId = -1;
    int trackIndex = -1;
    double tickStart = 0;
    double tickEnd = 0;
    double valueStart = 0;
    double valueEnd = 0;
    bool ticksAreLocal = false;

    [[nodiscard]] bool isValid() const;
};

struct HistoryFocusTransition {
    HistoryFocus before;
    HistoryFocus after;
};

enum class HistoryFocusVisibility {
    Unavailable,
    ScrollRequired,
    ContextSwitchRequired,
    Visible,
};

#endif // HISTORYFOCUS_H
