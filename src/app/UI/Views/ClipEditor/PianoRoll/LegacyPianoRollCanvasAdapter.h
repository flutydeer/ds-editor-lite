#ifndef LEGACYPIANOROLLCANVASADAPTER_H
#define LEGACYPIANOROLLCANVASADAPTER_H

#include "IPianoRollCanvas.h"

class PianoRollGraphicsScene;
class PianoRollGraphicsView;

class LegacyPianoRollCanvasAdapter final : public IPianoRollCanvas {
    Q_OBJECT

public:
    explicit LegacyPianoRollCanvasAdapter(QObject *parent = nullptr);

    [[nodiscard]] EditorCanvasBackend backend() const override;
    [[nodiscard]] QWidget *widget() const override;
    [[nodiscard]] QScrollBar *horizontalScrollBar() const override;
    [[nodiscard]] QScrollBar *verticalScrollBar() const override;
    void setDataContext(SingingClip *clip) override;
    void setEditMode(ClipEditorGlobal::PianoRollEditMode mode) override;
    void setTrackColorIndex(int index) override;
    [[nodiscard]] EditorViewportState viewportState() const override;
    void restoreViewportState(const EditorViewportState &state) override;
    bool centerAt(double tick, double keyIndex) override;
    bool setViewportScale(double horizontalScale, double verticalScale) override;
    [[nodiscard]] double startTick() const override;
    [[nodiscard]] double endTick() const override;
    [[nodiscard]] double centerKeyIndex() const override;
    [[nodiscard]] double scaleX() const override;
    [[nodiscard]] double scaleY() const override;
    [[nodiscard]] int horizontalBarValue() const override;
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
    void syncSelection() const;

    PianoRollGraphicsScene *m_scene = nullptr;
    PianoRollGraphicsView *m_view = nullptr;
};

#endif // LEGACYPIANOROLLCANVASADAPTER_H
