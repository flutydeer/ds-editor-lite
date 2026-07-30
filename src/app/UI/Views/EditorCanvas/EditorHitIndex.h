#ifndef EDITORHITINDEX_H
#define EDITORHITINDEX_H

#include "EditorCanvasTypes.h"

#include <QHash>

class EditorHitIndex final {
public:
    void rebuild(const EditorRenderSnapshot &snapshot);
    [[nodiscard]] EditorHitResult query(const QPointF &logicalPosition,
                                        double horizontalScale) const;
    void clear();

private:
    struct Entry {
        QRectF bounds;
        int objectId = -1;
        int zOrder = -1;
    };

    [[nodiscard]] int bucketFor(double y) const;

    QHash<int, QVector<Entry>> m_buckets;
    double m_bucketHeight = 1.0;
};

#endif // EDITORHITINDEX_H
