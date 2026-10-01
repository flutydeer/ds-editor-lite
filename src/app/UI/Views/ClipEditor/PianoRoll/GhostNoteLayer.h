#ifndef GHOSTNOTELAYER_H
#define GHOSTNOTELAYER_H

#include <QVector>

#include <limits>

#include "GhostNoteSource.h"
#include "UI/Views/Common/EditorRhiGeometry.h"

// Retained vertex cache for the ghost note bars of the RHI backend. Bars are emitted in
// scene coordinates and the camera offset is applied by the projection matrix, so pure
// scrolling never invalidates the geometry. The cache only goes stale when the ghost
// list, the palette, the scales, the host clip offset or the DPR change, or when the
// camera leaves the emitted window. rebuildSnapshot() splices the vertices in before the
// notes so the draw order keeps ghosts underneath.
class GhostNoteLayer final {
public:
    // Flags the retained geometry stale. Call on ghost list changes and on theme
    // switches — bar colors are baked into the vertices.
    void markDirty();

    // Brings the cache up to date and returns its vertices. The geometry arguments must
    // match what the renderer assumes: scene x of a local tick is
    // tick * pixelsPerTick + xAtZeroTick, scene y of a key row is
    // (127 - keyIndex) * rowHeight, and [localStart, localEnd] x [sceneTop, sceneBottom]
    // is the range the current frame must cover.
    [[nodiscard]] const QVector<EditorRhiSolidVertex> &
        ensureUpToDate(const GhostNoteSource *source, double clipStart, double xAtZeroTick,
                       double pixelsPerTick, double rowHeight, double dpr, double localStart,
                       double localEnd, double sceneTop, double sceneBottom);

private:
    void rebuild(const GhostNoteSource *source, double clipStart, double xAtZeroTick,
                 double pixelsPerTick, double rowHeight, double dpr, double localStart,
                 double localEnd, double sceneTop, double sceneBottom);

    QVector<EditorRhiSolidVertex> m_vertices;
    bool m_dirty = true;
    // Parameters the retained geometry was built with; any mismatch triggers a rebuild.
    // The coverage window starts empty, so the first frame always emits.
    bool m_enabled = false;
    double m_dpr = 0.0;
    double m_pixelsPerTick = 0.0;
    double m_rowHeight = 0.0;
    double m_clipStart = 0.0;
    double m_xAtZeroTick = 0.0;
    double m_fromTick = std::numeric_limits<double>::infinity();
    double m_toTick = -std::numeric_limits<double>::infinity();
    double m_bandTop = std::numeric_limits<double>::infinity();
    double m_bandBottom = -std::numeric_limits<double>::infinity();
};

#endif // GHOSTNOTELAYER_H
