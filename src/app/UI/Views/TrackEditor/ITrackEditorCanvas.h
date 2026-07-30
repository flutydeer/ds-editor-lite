#ifndef ITRACKEDITORCANVAS_H
#define ITRACKEDITORCANVAS_H

#include "UI/Views/EditorCanvas/EditorCanvasTypes.h"

#include <lite/History/HistoryFocus.h>

#include <QObject>

class QScrollBar;
class QWheelEvent;
class QWidget;

class ITrackEditorCanvas : public QObject {
    Q_OBJECT

public:
    explicit ITrackEditorCanvas(QObject *parent = nullptr) : QObject(parent) {
    }

    ~ITrackEditorCanvas() override = default;

    [[nodiscard]] virtual EditorCanvasBackend backend() const = 0;
    [[nodiscard]] virtual QWidget *widget() const = 0;
    [[nodiscard]] virtual QScrollBar *horizontalScrollBar() const = 0;
    [[nodiscard]] virtual QScrollBar *verticalScrollBar() const = 0;
    [[nodiscard]] virtual EditorViewportState viewportState() const = 0;
    virtual void restoreViewportState(const EditorViewportState &state) = 0;
    virtual bool centerAt(double tick, double trackIndex) = 0;
    virtual bool setViewportScale(double horizontalScale, double verticalScale) = 0;
    [[nodiscard]] virtual QRectF visibleRect() const = 0;
    [[nodiscard]] virtual double sceneXForTick(double tick) const = 0;
    virtual void setSceneLength(int tick) = 0;
    virtual void setPlaybackPosition(double tick) = 0;
    virtual void setLastPlaybackPosition(double tick) = 0;
    virtual void refreshSnapshot(EditorDirtyDomains domains) = 0;
    [[nodiscard]] virtual HistoryFocusVisibility
        focusVisibility(const HistoryFocus &focus) const = 0;
    virtual bool revealFocus(const HistoryFocus &focus, bool animated) = 0;

public slots:
    virtual void onWheelHorScale(QWheelEvent *event) = 0;
    virtual void onWheelVerScale(QWheelEvent *event) = 0;
    virtual void onWheelHorScroll(QWheelEvent *event) = 0;
    virtual void onWheelVerScroll(QWheelEvent *event) = 0;

signals:
    void scaleChanged(double horizontalScale, double verticalScale);
    void timeRangeChanged(double startTick, double endTick);
    void visibleRectChanged(const QRectF &rect);
    void sizeChanged(QSize size);
    void rendererFailed(const QString &reason);
};

#endif // ITRACKEDITORCANVAS_H
