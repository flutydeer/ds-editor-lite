//
// Created by fluty on 2024/2/10.
//

#ifndef PIANOROLLTOOLBARVIEW_H
#define PIANOROLLTOOLBARVIEW_H

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QWidget>

class SeekBar;
class QAbstractButton;
class Clip;
class Button;

using namespace ClipEditorGlobal;

class ClipEditorToolBarViewPrivate;

class ClipEditorToolBarView final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor)
    Q_PROPERTY(QColor iconDisabledColor READ iconDisabledColor WRITE setIconDisabledColor)
    Q_PROPERTY(QColor iconOnColor READ iconOnColor WRITE setIconOnColor)
    Q_PROPERTY(QColor iconOnDisabledColor READ iconOnDisabledColor WRITE setIconOnDisabledColor)

public:
    explicit ClipEditorToolBarView(QWidget *parent = nullptr);
    void setDataContext(Clip *clip);
    [[nodiscard]] PianoRollEditMode editMode() const;

signals:
    void editModeChanged(PianoRollEditMode mode);

protected:
    void changeEvent(QEvent *event) override;

private:
    Q_DECLARE_PRIVATE(ClipEditorToolBarView)
    ClipEditorToolBarViewPrivate *d_ptr;

    // Theme color accessors (QSS-overridable via qproperty-*); setters
    // re-tint the already-generated button icons
    [[nodiscard]] QColor iconColor() const;
    void setIconColor(const QColor &color);
    [[nodiscard]] QColor iconDisabledColor() const;
    void setIconDisabledColor(const QColor &color);
    [[nodiscard]] QColor iconOnColor() const;
    void setIconOnColor(const QColor &color);
    [[nodiscard]] QColor iconOnDisabledColor() const;
    void setIconOnDisabledColor(const QColor &color);
};



#endif // PIANOROLLTOOLBARVIEW_H
