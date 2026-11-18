#ifndef DATASET_TOOLS_TRACKSGRAPHICSSCENE_H
#define DATASET_TOOLS_TRACKSGRAPHICSSCENE_H

#include "UI/Views/Common/TimeGraphicsScene.h"

// Drag-and-drop hit result for the track canvas. Unlike trackIndexAt(),
// which only ever hits real tracks, this also resolves the virtual append
// slot at the bottom of the canvas.
struct TrackDropSlot {
    enum class Kind { ExistingTrack, Append };
    Kind kind = Kind::ExistingTrack;
    // Real track index for ExistingTrack; track count (the next row) for
    // Append, used as the geometric row index of the append slot.
    int trackIndex = -1;
    int snappedTick = 0;
};

class TracksGraphicsScene final : public TimeGraphicsScene {
public:
    explicit TracksGraphicsScene();
    int trackIndexAt(double sceneY) const;
    // Geometric row under sceneY for a clip gesture: real track rows first,
    // then the virtual append row at the track count. Anything below the
    // append row clamps to it, so the empty area past the last row still
    // targets the slot. Returns -1 above the first row.
    int dragRowAt(double sceneY) const;
    int tickAt(double sceneX) const;
    // True when sceneY falls inside the virtual append slot or the empty area
    // below it. Never matches real tracks.
    bool isAppendSlotAt(double sceneY) const;

public slots:
    void onViewResized(QSize size);
    void onTrackCountChanged(int count);

private:
    void updateSceneRect() override;

    int m_trackCount = 0;
    QSize m_graphicsViewSize = QSize(0, 0);
};


#endif // DATASET_TOOLS_TRACKSGRAPHICSSCENE_H
