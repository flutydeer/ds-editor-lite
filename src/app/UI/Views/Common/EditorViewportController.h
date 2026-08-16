#ifndef EDITORVIEWPORTCONTROLLER_H
#define EDITORVIEWPORTCONTROLLER_H

#include "EditorViewportAnimation.h"

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

class EditorViewportController final : public QObject {
    Q_OBJECT

public:
    struct State {
        double centerTick = 0.0;
        double centerUnit = 0.0;
        double horizontalScale = 1.0;
        double verticalScale = 1.0;

        bool operator==(const State &) const = default;
    };

    explicit EditorViewportController(QObject *parent = nullptr);

    void setPixelsPerQuarterNote(double value);
    void setContentTickRange(double startTick, double endTick);
    void setVerticalContent(double unitCount, double unitHeight);
    void setViewportSize(const QSizeF &size);
    void setScaleBounds(double minX, double maxX, double minY, double maxY);
    void setEnsureContentFillsViewport(bool horizontal, bool vertical);
    // Left margin in screen pixels before the content start, kept inside the
    // scene so the playhead indicator never clips at the left viewport edge.
    void setLeftMarginPx(double px);

    [[nodiscard]] State state() const;
    bool restoreState(const State &state);
    bool setScale(double horizontal, double vertical, const QPointF &anchor);
    bool centerAt(double tick, double unit, bool animated = false);
    bool ensureVisible(const QRectF &rect, double xMargin, double yMargin, bool animated = false);
    bool setStartTick(double tick);
    bool setOffset(const QPointF &offset, bool animated = false);
    void scrollBy(const QPointF &deltaPixels);
    void stopAnimation();

    [[nodiscard]] double horizontalScale() const;
    [[nodiscard]] double verticalScale() const;
    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double topUnit() const;
    [[nodiscard]] double bottomUnit() const;
    [[nodiscard]] double horizontalOffset() const;
    [[nodiscard]] double verticalOffset() const;
    [[nodiscard]] QPointF offset() const;
    [[nodiscard]] double maximumOffset(Qt::Orientation orientation) const;
    [[nodiscard]] double boundedScale(Qt::Orientation orientation, double requested) const;
    [[nodiscard]] QRectF visibleSceneRect() const;
    [[nodiscard]] QRectF logicalVisibleSceneRect() const;
    [[nodiscard]] QSizeF viewportSize() const;

    [[nodiscard]] double tickToSceneX(double tick) const;
    [[nodiscard]] double sceneXToTick(double x) const;
    [[nodiscard]] double unitToSceneY(double unit) const;
    [[nodiscard]] double sceneYToUnit(double y) const;
    [[nodiscard]] QPointF viewportToScene(const QPointF &position) const;

signals:
    void viewportChanged();
    void scaleChanged(double horizontal, double vertical);
    void timeRangeChanged(double startTick, double endTick);
    void verticalRangeChanged(double topUnit, double bottomUnit);
    void scrollChanged(double horizontalOffset, double verticalOffset);

private:
    void normalize(bool scaleChanged);
    void notify(bool emitScaleChanged);
    void applyOffset(const QPointF &offset);
    [[nodiscard]] double effectiveMinimumScaleX() const;
    [[nodiscard]] double effectiveMinimumScaleY() const;
    [[nodiscard]] double contentWidth() const;
    [[nodiscard]] double contentHeight() const;
    [[nodiscard]] double pixelsPerTick() const;

    double m_pixelsPerQuarterNote = 64.0;
    double m_startTick = 0.0;
    double m_endTick = 0.0;
    double m_leftMarginPx = 0.0;
    double m_unitCount = 0.0;
    double m_unitHeight = 1.0;
    QSizeF m_viewportSize;
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
    double m_minScaleX = 0.0001;
    double m_maxScaleX = 10000.0;
    double m_minScaleY = 0.0001;
    double m_maxScaleY = 8.0;
    double m_offsetX = 0.0;
    double m_offsetY = 0.0;
    bool m_fillX = true;
    bool m_fillY = true;
    EditorViewportAnimation m_offsetAnimation;
};

#endif // EDITORVIEWPORTCONTROLLER_H
