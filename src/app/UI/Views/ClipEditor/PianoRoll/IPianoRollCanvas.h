#ifndef IPIANOROLLCANVAS_H
#define IPIANOROLLCANVAS_H

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/EditorCanvas/EditorCanvasTypes.h"

#include <lite/History/HistoryFocus.h>

#include <QObject>

class QScrollBar;
class QWheelEvent;
class SingingClip;
class QWidget;

class IPianoRollCanvas : public QObject {
    Q_OBJECT

public:
    explicit IPianoRollCanvas(QObject *parent = nullptr) : QObject(parent) {
    }

    ~IPianoRollCanvas() override = default;

    [[nodiscard]] virtual EditorCanvasBackend backend() const = 0;
    [[nodiscard]] virtual QWidget *widget() const = 0;
    [[nodiscard]] virtual QScrollBar *horizontalScrollBar() const = 0;
    [[nodiscard]] virtual QScrollBar *verticalScrollBar() const = 0;
    virtual void setDataContext(SingingClip *clip) = 0;
    virtual void setEditMode(ClipEditorGlobal::PianoRollEditMode mode) = 0;
    virtual void setTrackColorIndex(int index) = 0;
    [[nodiscard]] virtual EditorViewportState viewportState() const = 0;
    virtual void restoreViewportState(const EditorViewportState &state) = 0;
    virtual bool centerAt(double tick, double keyIndex) = 0;
    virtual bool setViewportScale(double horizontalScale, double verticalScale) = 0;
    [[nodiscard]] virtual double startTick() const = 0;
    [[nodiscard]] virtual double endTick() const = 0;
    [[nodiscard]] virtual double centerKeyIndex() const = 0;
    [[nodiscard]] virtual double scaleX() const = 0;
    [[nodiscard]] virtual double scaleY() const = 0;
    [[nodiscard]] virtual int horizontalBarValue() const = 0;
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
    void keyRangeChanged(double topKey, double bottomKey);
    void keyHovered(int keyIndex);
    void keyHoverCleared();
    void rendererFailed(const QString &reason);
};

#endif // IPIANOROLLCANVAS_H
