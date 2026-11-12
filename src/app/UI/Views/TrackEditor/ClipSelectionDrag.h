#ifndef CLIPSELECTIONDRAG_H
#define CLIPSELECTIONDRAG_H

#include <lite/ProjectModel/AppModel/Clip.h>

#include <QList>

#include <functional>

class Timeline;

// Planning for one pointer gesture over the clip selection in the arrangement. Both render
// backends feed it the same numbers (pointer position expressed in ticks and track rows) and read
// back per-clip targets, so the gesture math exists exactly once while each backend keeps owning
// its own preview and commit plumbing.
//
// The planner never touches the model: it works from the snapshots taken when the pointer went
// down, and its output is what the view previews and what the controller later commits. The
// millisecond truth of realtime-anchored audio clips is re-projected through the same
// AudioClipDragState the commit path uses, so a preview never disagrees with the committed result.
namespace ClipSelectionDrag {

    // One clip of the gesture, captured when the pointer went down.
    struct ClipState {
        int clipId = -1;
        int trackIndex = -1;
        Clip::ClipCommonProperties properties;
        // Singing clips may be extended past their content by the right edge.
        bool lengthResizable = false;
        // Furthest content end in ticks. Only meaningful for singing clips.
        int contentLength = 0;
        // Audio clips carrying a realtime anchor. When set, `properties` must also carry the
        // millisecond truth, and the planner keeps it in sync while moving or trimming.
        bool realTimeAnchored = false;
    };

    // Target geometry of one clip for the current pointer position.
    struct Placement {
        int clipId = -1;
        int trackIndex = -1;
        Clip::ClipCommonProperties properties;
    };

    enum class Gesture { Move, ResizeLeft, ResizeRight };

    class Session final {
    public:
        struct Environment {
            int trackCount = 0;
            const Timeline *timeline = nullptr;
            // Grid step, in ticks, at the requested tick. Mirrors the view's WYSIWYG grid.
            std::function<int(int)> gridStepAt;
        };

        // Captures the selection. The gesture is ignored (and the session stays inactive) when the
        // anchor is not part of `clips`.
        void begin(Gesture gesture, int anchorClipId, const QList<ClipState> &clips,
                   const Environment &environment, double pressTick, int pressTrackIndex);
        // Translates one pointer position into per-clip targets. Returns true when the targets
        // changed since the previous call, so a view can skip a repaint.
        bool update(double pointerTick, int pointerTrackIndex, bool snapOff);
        void reset();

        [[nodiscard]] bool active() const {
            return m_active;
        }

        // True once the gesture actually moved or trimmed at least one clip.
        [[nodiscard]] bool moved() const {
            return m_moved;
        }

        [[nodiscard]] Gesture gesture() const {
            return m_gesture;
        }

        [[nodiscard]] int anchorClipId() const {
            return m_anchorClipId;
        }

        [[nodiscard]] const QList<ClipState> &originals() const {
            return m_originals;
        }

        [[nodiscard]] const QList<Placement> &placements() const {
            return m_placements;
        }

    private:
        [[nodiscard]] const ClipState *originalFor(int clipId) const;
        [[nodiscard]] int gridStep(int tick, bool snapOff) const;
        [[nodiscard]] int snapTick(int tick, bool snapOff) const;
        void planMove(int appliedTicks, int appliedTracks, bool snapOff);
        void planResize(int appliedTicks, bool snapOff);

        bool m_active = false;
        bool m_moved = false;
        Gesture m_gesture = Gesture::Move;
        int m_anchorClipId = -1;
        double m_pressTick = 0.0;
        int m_pressTrackIndex = -1;
        QList<ClipState> m_originals;
        QList<Placement> m_placements;
        Environment m_environment;
    };

} // namespace ClipSelectionDrag

#endif // CLIPSELECTIONDRAG_H
