//
// Created by fluty on 2024/2/10.
//

#ifndef PIANOROLLTOOLBARVIEW_H
#define PIANOROLLTOOLBARVIEW_H

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QWidget>

class EditLabel;
class SeekBar;
class QAbstractButton;
class Clip;
class Button;

using namespace ClipEditorGlobal;

class ClipEditorToolBarViewPrivate;

class ClipEditorToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ClipEditorToolBarView(QWidget *parent = nullptr);
    void setDataContext(Clip *clip);
    [[nodiscard]] PianoRollEditMode editMode() const;

signals:
    void editModeChanged(PianoRollEditMode mode);

private:
    Q_DECLARE_PRIVATE(ClipEditorToolBarView)
    ClipEditorToolBarViewPrivate *d_ptr;
};



#endif // PIANOROLLTOOLBARVIEW_H
