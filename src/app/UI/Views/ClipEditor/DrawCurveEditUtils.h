#ifndef DRAWCURVEEDITUTILS_H
#define DRAWCURVEEDITUTILS_H

#include <QList>
#include <QPoint>

#include <functional>
#include <optional>
#include <utility>

class DrawCurve;

namespace DrawCurveEditUtils {
    using ValueProvider = std::function<std::optional<int>(int)>;

    struct StrokeState {
        QPoint mouseDownPosition;
        DrawCurve *editingCurve = nullptr;
        bool drawOnInterval = false;
        bool newCurveCreated = false;
    };

    [[nodiscard]] std::pair<int, int> strokeTickRange(int previousTick, int currentTick);
    [[nodiscard]] StrokeState beginStroke(const QList<DrawCurve *> &curves,
                                          const QPoint &mouseDownPosition);
    bool updateStroke(QList<DrawCurve *> &curves, StrokeState &state,
                      const QPoint &previousPosition, const QPoint &currentPosition,
                      const ValueProvider &valueAtTick);
    [[nodiscard]] std::optional<int> generatedValueAt(const QList<DrawCurve *> &curves, int tick);
}

#endif // DRAWCURVEEDITUTILS_H
