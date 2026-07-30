#ifndef SCREENSPACESTROKETESSELLATOR_H
#define SCREENSPACESTROKETESSELLATOR_H

#include "EditorCanvasTypes.h"

#include <QColor>
#include <QPointF>
#include <QVector>

struct ScreenSpaceStrokeVertex {
    QPointF position;
    QColor color;
};

class ScreenSpaceStrokeTessellator final {
public:
    [[nodiscard]] static QVector<ScreenSpaceStrokeVertex>
        tessellate(const QVector<QPointF> &points, const QColor &color, float width, double scaleX,
                   double scaleY, double devicePixelRatio,
                   EditorStrokeJoin join = EditorStrokeJoin::Round,
                   EditorStrokeCap cap = EditorStrokeCap::Round);

private:
    ScreenSpaceStrokeTessellator() = delete;
};

#endif // SCREENSPACESTROKETESSELLATOR_H
