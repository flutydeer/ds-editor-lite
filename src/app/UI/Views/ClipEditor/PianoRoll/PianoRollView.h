//
// Created by fluty on 24-8-21.
//

#ifndef PIANOROLLVIEW_H
#define PIANOROLLVIEW_H

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QPointer>
#include <QWidget>

class IPianoRollCanvas;
class PhonemeView;
class QLabel;
class SingingClip;
class PianoKeyboardView;
class QVBoxLayout;
class QWheelEvent;
class TimelineView;

class PianoRollView final : public QWidget {
    Q_OBJECT

public:
    explicit PianoRollView(QWidget *parent = nullptr);
    [[nodiscard]] IPianoRollCanvas *canvas() const;
    void setDataContext(SingingClip *clip);
    void setTrackColorIndex(int index);
    void setPlaybackPosition(double tick) const;
    void setLastPlaybackPosition(double tick) const;

public slots:
    void onEditModeChanged(ClipEditorGlobal::PianoRollEditMode mode);
    void onWheelHorScale(QWheelEvent *event) const;
    void onWheelHorScroll(QWheelEvent *event) const;

protected:
    void changeEvent(QEvent *event) override;

private:
    void connectCanvas();
    void scheduleLegacyFallback(const QString &reason);
    void replaceCanvasWithLegacy();

    IPianoRollCanvas *m_canvas = nullptr;
    PianoKeyboardView *m_keyboardView;
    TimelineView *m_timelineView;
    PhonemeView *m_phonemeView;
    QLabel *m_lbTip;
    QVBoxLayout *m_rightLayout = nullptr;
    QPointer<SingingClip> m_clip;
    ClipEditorGlobal::PianoRollEditMode m_editMode = ClipEditorGlobal::Select;
    int m_trackColorIndex = 0;
    bool m_fallbackPending = false;

signals:
    void canvasScaleChanged(double horizontalScale, double verticalScale);
    void horizontalScrollValueChanged(int value);
};



#endif // PIANOROLLVIEW_H
