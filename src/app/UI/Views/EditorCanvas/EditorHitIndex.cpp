#include "EditorHitIndex.h"

#include "Global/AppGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <algorithm>
#include <cmath>

void EditorHitIndex::rebuild(const EditorRenderSnapshot &snapshot) {
    clear();
    m_bucketHeight = snapshot.kind == EditorCanvasKind::TrackEditor
                         ? static_cast<double>(TracksEditorGlobal::trackHeight)
                         : ClipEditorGlobal::noteHeight;
    for (qsizetype i = 0; i < snapshot.rectangles.size(); ++i) {
        const auto &rect = snapshot.rectangles.at(i);
        if (rect.objectId < 0 || rect.bounds.isEmpty())
            continue;
        const auto firstBucket = bucketFor(rect.bounds.top());
        const auto lastBucket = bucketFor(rect.bounds.bottom());
        for (auto bucket = firstBucket; bucket <= lastBucket; ++bucket) {
            m_buckets[bucket].append({
                .bounds = rect.bounds,
                .objectId = rect.objectId,
                .zOrder = rect.layer * 100000 + static_cast<int>(i),
            });
        }
    }
    for (auto &entries : m_buckets) {
        std::ranges::sort(entries, [](const Entry &left, const Entry &right) {
            if (!qFuzzyCompare(left.bounds.left(), right.bounds.left()))
                return left.bounds.left() < right.bounds.left();
            return left.zOrder < right.zOrder;
        });
    }
}

EditorHitResult EditorHitIndex::query(const QPointF &logicalPosition,
                                      const double horizontalScale) const {
    const auto it = m_buckets.constFind(bucketFor(logicalPosition.y()));
    if (it == m_buckets.cend())
        return {};

    const auto &entries = it.value();
    const auto edgeTolerance = AppGlobal::resizeTolerance / qMax(0.01, horizontalScale);
    const Entry *best = nullptr;
    for (const auto &entry : entries) {
        if (entry.bounds.left() > logicalPosition.x())
            break;
        if (entry.bounds.right() < logicalPosition.x() || !entry.bounds.contains(logicalPosition))
            continue;
        if (!best || entry.zOrder > best->zOrder)
            best = &entry;
    }
    if (!best)
        return {};

    auto part = EditorHitResult::Part::Body;
    if (std::abs(logicalPosition.x() - best->bounds.left()) <= edgeTolerance)
        part = EditorHitResult::Part::LeftEdge;
    else if (std::abs(logicalPosition.x() - best->bounds.right()) <= edgeTolerance)
        part = EditorHitResult::Part::RightEdge;
    return {.objectId = best->objectId, .part = part};
}

void EditorHitIndex::clear() {
    m_buckets.clear();
}

int EditorHitIndex::bucketFor(const double y) const {
    return static_cast<int>(std::floor(y / qMax(1.0, m_bucketHeight)));
}
