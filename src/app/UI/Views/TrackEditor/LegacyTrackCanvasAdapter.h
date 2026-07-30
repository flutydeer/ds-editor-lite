#ifndef LEGACYTRACKCANVASADAPTER_H
#define LEGACYTRACKCANVASADAPTER_H

#include "ITrackEditorCanvas.h"

#include <memory>

class LegacyTrackCanvasAdapter final : public ITrackEditorCanvas {
    Q_OBJECT

public:
    explicit LegacyTrackCanvasAdapter(QObject *parent = nullptr);
    ~LegacyTrackCanvasAdapter() override;

    [[nodiscard]] EditorCanvasBackend backend() const override;
    [[nodiscard]] QWidget *widget() const override;
    [[nodiscard]] QScrollBar *horizontalScrollBar() const override;
    [[nodiscard]] QScrollBar *verticalScrollBar() const override;
    [[nodiscard]] EditorViewportState viewportState() const override;
    void restoreViewportState(const EditorViewportState &state) override;
    bool centerAt(double tick, double trackIndex) override;
    bool setViewportScale(double horizontalScale, double verticalScale) override;
    [[nodiscard]] QRectF visibleRect() const override;
    [[nodiscard]] double sceneXForTick(double tick) const override;
    void setSceneLength(int tick) override;
    void setPlaybackPosition(double tick) override;
    void setLastPlaybackPosition(double tick) override;
    void refreshSnapshot(EditorDirtyDomains domains) override;
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const override;
    bool revealFocus(const HistoryFocus &focus, bool animated) override;

public slots:
    void onWheelHorScale(QWheelEvent *event) override;
    void onWheelVerScale(QWheelEvent *event) override;
    void onWheelHorScroll(QWheelEvent *event) override;
    void onWheelVerScroll(QWheelEvent *event) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // LEGACYTRACKCANVASADAPTER_H
