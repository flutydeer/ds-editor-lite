//
// Created by fluty on 24-8-21.
//

#ifndef PIANOROLLVIEW_H
#define PIANOROLLVIEW_H

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QWidget>

class QLabel;
class SingingClip;
class PianoRollGraphicsScene;
class PianoRollGraphicsView;
class PianoKeyboardView;
class TimelineView;

class PianoRollView final : public QWidget {
    Q_OBJECT

public:
    explicit PianoRollView(QWidget *parent = nullptr);
    [[nodiscard]] PianoRollGraphicsView *graphicsView() const;
    void setDataContext(SingingClip *clip) const;

public slots:
    void onEditModeChanged(ClipEditorGlobal::PianoRollEditMode mode) const;

private:
    PianoRollGraphicsScene *m_scene;
    PianoRollGraphicsView *m_graphicsView;
    PianoKeyboardView *m_keyboardView;
    TimelineView *m_timelineView;
    QLabel *m_lbTip;
};



#endif // PIANOROLLVIEW_H
