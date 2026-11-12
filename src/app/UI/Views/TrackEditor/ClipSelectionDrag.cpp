#include "ClipSelectionDrag.h"

#include "AudioClipDragState.h"

#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/Utils/ClipResizeUtils.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ClipSelectionDrag {

    namespace {

        int visibleStart(const Clip::ClipCommonProperties &properties) {
            return properties.start + properties.clipStart;
        }

        bool sameGeometry(const Clip::ClipCommonProperties &left,
                          const Clip::ClipCommonProperties &right) {
            return left.start == right.start && left.length == right.length &&
                   left.clipStart == right.clipStart && left.clipLen == right.clipLen;
        }

        // The same shape the command layer validates before committing, so the planner never hands
        // the view a target the repository would reject.
        bool validGeometry(const Clip::ClipCommonProperties &properties) {
            const auto localEnd = static_cast<qint64>(properties.clipStart) + properties.clipLen;
            const auto start = static_cast<qint64>(properties.start) + properties.clipStart;
            const auto end = start + properties.clipLen;
            return localEnd <= std::numeric_limits<int>::max() && start >= 0 &&
                   end <= std::numeric_limits<int>::max() && properties.length >= 0 &&
                   properties.clipStart >= 0 && properties.clipLen >= 0;
        }

    } // namespace

    void Session::begin(const Gesture gesture, const int anchorClipId,
                        const QList<ClipState> &clips, const Environment &environment,
                        const double pressTick, const int pressTrackIndex) {
        reset();
        if (clips.isEmpty() || anchorClipId < 0)
            return;
        const auto anchor =
            std::find_if(clips.cbegin(), clips.cend(), [anchorClipId](const ClipState &clip) {
                return clip.clipId == anchorClipId;
            });
        if (anchor == clips.cend())
            return;

        m_active = true;
        m_gesture = gesture;
        m_anchorClipId = anchorClipId;
        m_originals = clips;
        m_environment = environment;
        m_pressTick = pressTick;
        m_pressTrackIndex = pressTrackIndex >= 0 ? pressTrackIndex : anchor->trackIndex;
    }

    void Session::reset() {
        m_active = false;
        m_moved = false;
        m_gesture = Gesture::Move;
        m_anchorClipId = -1;
        m_pressTick = 0.0;
        m_pressTrackIndex = -1;
        m_originals.clear();
        m_placements.clear();
        m_environment = {};
    }

    bool Session::update(const double pointerTick, const int pointerTrackIndex,
                         const bool snapOff) {
        if (!m_active)
            return false;
        const auto rawTicks = static_cast<int>(std::llround(pointerTick - m_pressTick));
        if (m_gesture == Gesture::Move)
            planMove(rawTicks, pointerTrackIndex - m_pressTrackIndex, snapOff);
        else
            planResize(rawTicks, snapOff);

        bool moved = false;
        for (const auto &placement : m_placements) {
            const auto *original = originalFor(placement.clipId);
            if (!original)
                continue;
            moved = moved || placement.trackIndex != original->trackIndex ||
                    !sameGeometry(placement.properties, original->properties);
        }
        m_moved = moved;
        return true;
    }

    const ClipState *Session::originalFor(const int clipId) const {
        for (const auto &clip : m_originals) {
            if (clip.clipId == clipId)
                return &clip;
        }
        return nullptr;
    }

    int Session::gridStep(const int tick, const bool snapOff) const {
        if (snapOff || !m_environment.gridStepAt)
            return 1;
        const auto step = m_environment.gridStepAt(tick);
        return step > 0 ? step : 1;
    }

    int Session::snapTick(const int tick, const bool snapOff) const {
        if (!m_environment.timeline)
            return tick;
        return TimelineSnapUtils::snapNearest(tick, gridStep(tick, snapOff),
                                              *m_environment.timeline);
    }

    void Session::planMove(const int rawTicks, const int rawTracks, const bool snapOff) {
        const auto *anchor = originalFor(m_anchorClipId);
        if (!anchor) {
            reset();
            return;
        }
        const auto trackCount = std::max(0, m_environment.trackCount);
        if (trackCount <= 0) {
            m_placements.clear();
            return;
        }

        int minimumVisibleStart = 0;
        int minimumTrackIndex = 0;
        int maximumTrackIndex = 0;
        bool first = true;
        for (const auto &clip : m_originals) {
            const auto left = visibleStart(clip.properties);
            if (first) {
                minimumVisibleStart = left;
                minimumTrackIndex = clip.trackIndex;
                maximumTrackIndex = clip.trackIndex;
                first = false;
            } else {
                minimumVisibleStart = std::min(minimumVisibleStart, left);
                minimumTrackIndex = std::min(minimumTrackIndex, clip.trackIndex);
                maximumTrackIndex = std::max(maximumTrackIndex, clip.trackIndex);
            }
        }

        // The block is rigid: clamp it as a whole, snap the anchor, then clamp again because
        // snapping can round back below the block limit. Every clip then takes the same delta.
        const auto anchorLeft = visibleStart(anchor->properties);
        auto applied = std::max(rawTicks, -minimumVisibleStart);
        applied = snapTick(anchorLeft + applied, snapOff) - anchorLeft;
        applied = std::max(applied, -minimumVisibleStart);

        const auto lowestTrackDelta = -minimumTrackIndex;
        const auto highestTrackDelta = trackCount - 1 - maximumTrackIndex;
        const auto appliedTracks = highestTrackDelta < lowestTrackDelta
                                       ? 0
                                       : std::clamp(rawTracks, lowestTrackDelta, highestTrackDelta);

        m_placements.clear();
        m_placements.reserve(m_originals.size());
        for (const auto &clip : m_originals) {
            Placement placement{clip.clipId, clip.trackIndex + appliedTracks, clip.properties};
            const auto left = visibleStart(clip.properties) + applied;
            if (clip.realTimeAnchored && m_environment.timeline) {
                // Same projection the move command recomputes, so preview and commit agree to the
                // tick even across tempo changes.
                auto drag = AudioClipDragState::begin(
                    clip.properties.trimStartMs, clip.properties.playLengthMs,
                    clip.properties.materialLengthMs, left, left, *m_environment.timeline);
                drag.moveTo(left, placement.properties, *m_environment.timeline);
                drag.writeTruth(placement.properties);
            } else {
                placement.properties.start = left - clip.properties.clipStart;
            }
            if (!validGeometry(placement.properties))
                continue;
            if (placement.trackIndex < 0 || placement.trackIndex >= trackCount)
                continue;
            m_placements.append(placement);
        }
    }

    void Session::planResize(const int rawTicks, const bool snapOff) {
        const auto *anchor = originalFor(m_anchorClipId);
        if (!anchor) {
            reset();
            return;
        }

        // Only the anchor's edge snaps. The resulting delta is what the rest of the selection
        // follows, which keeps the block's edges aligned instead of snapping each clip separately.
        const auto anchorLeft = visibleStart(anchor->properties);
        const auto anchorRight = anchorLeft + anchor->properties.clipLen;
        const auto anchorEdge = m_gesture == Gesture::ResizeLeft ? anchorLeft : anchorRight;
        const auto applied = snapTick(anchorEdge + rawTicks, snapOff) - anchorEdge;

        m_placements.clear();
        m_placements.reserve(m_originals.size());
        for (const auto &clip : m_originals) {
            const auto left = visibleStart(clip.properties);
            const auto right = left + clip.properties.clipLen;
            const auto requested = (m_gesture == Gesture::ResizeLeft ? left : right) + applied;
            const auto minimumLength = gridStep(requested, snapOff);
            Placement placement{clip.clipId, clip.trackIndex, clip.properties};
            bool accepted;
            if (m_gesture == Gesture::ResizeLeft) {
                if (clip.realTimeAnchored && m_environment.timeline) {
                    auto drag = AudioClipDragState::begin(
                        clip.properties.trimStartMs, clip.properties.playLengthMs,
                        clip.properties.materialLengthMs, left, left, *m_environment.timeline);
                    accepted = drag.resizeLeftTo(requested, right, minimumLength,
                                                 placement.properties, *m_environment.timeline);
                    if (accepted)
                        drag.writeTruth(placement.properties);
                } else {
                    accepted = ClipResizeUtils::updateLeftEdge(placement.properties, requested,
                                                               minimumLength);
                }
            } else {
                if (clip.realTimeAnchored && m_environment.timeline) {
                    auto drag = AudioClipDragState::begin(
                        clip.properties.trimStartMs, clip.properties.playLengthMs,
                        clip.properties.materialLengthMs, left, left, *m_environment.timeline);
                    accepted = drag.resizeRightTo(requested, left, minimumLength,
                                                  placement.properties, *m_environment.timeline);
                    if (accepted)
                        drag.writeTruth(placement.properties);
                } else {
                    accepted = ClipResizeUtils::updateRightEdge(
                        placement.properties, requested - left, minimumLength, clip.lengthResizable,
                        clip.contentLength);
                }
            }
            if (!accepted || !validGeometry(placement.properties))
                continue;
            m_placements.append(placement);
        }
    }

} // namespace ClipSelectionDrag
