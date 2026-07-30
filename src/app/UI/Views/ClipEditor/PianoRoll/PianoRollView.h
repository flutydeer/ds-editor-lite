//
// Created by fluty on 24-8-21.
//

#ifndef PIANOROLLVIEW_H
#define PIANOROLLVIEW_H

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Interface/EditorViewState.h"

#include <lite/History/HistoryFocus.h>

#include <QWidget>

class PhonemeView;
class QLabel;
class SingingClip;
class PianoRollGraphicsScene;
class PianoRollGraphicsView;
class PianoRollRhiWidget;
class PianoKeyboardView;
class QVBoxLayout;
class QWheelEvent;
class TimelineView;

class PianoRollView final : public QWidget {
    Q_OBJECT

public:
    explicit PianoRollView(QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip) const;
    void setTrackColorIndex(int index) const;
    [[nodiscard]] PianoRollViewState viewState() const;
    bool centerAt(double tick, double keyIndex) const;
    bool setViewScale(double horizontalScale, double verticalScale) const;
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus, bool animated = true) const;
    [[nodiscard]] double scaleX() const;
    [[nodiscard]] int horizontalBarValue() const;

public slots:
    void onEditModeChanged(ClipEditorGlobal::PianoRollEditMode mode) const;
    void onWheelHorScale(QWheelEvent *event) const;
    void onWheelHorScroll(QWheelEvent *event) const;
    void setHorizontalBarValue(int value) const;
    void setPlaybackPosition(double tick) const;
    void setLastPlaybackPosition(double tick) const;

signals:
    void scaleChanged(double horizontal, double vertical);
    void horizontalBarValueChanged(int value);

protected:
    void changeEvent(QEvent *event) override;

private:
    void createLegacyBackend();
    void connectLegacyBackend();
    void connectRhiBackend();
    void fallbackToLegacy();
    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double topKeyIndex() const;
    [[nodiscard]] double bottomKeyIndex() const;

    PianoRollGraphicsScene *m_scene = nullptr;
    PianoRollGraphicsView *m_graphicsView = nullptr;
    PianoRollRhiWidget *m_rhiView = nullptr;
    QWidget *m_editorWidget = nullptr;
    PianoKeyboardView *m_keyboardView = nullptr;
    TimelineView *m_timelineView = nullptr;
    PhonemeView *m_phonemeView = nullptr;
    QLabel *m_lbTip = nullptr;
    QVBoxLayout *m_rightLayout = nullptr;
    mutable SingingClip *m_clip = nullptr;
    mutable int m_trackColorIndex = 0;
    mutable ClipEditorGlobal::PianoRollEditMode m_editMode = ClipEditorGlobal::Select;
};



#endif // PIANOROLLVIEW_H
