//
// Created by fluty on 2024/2/10.
//

#ifndef PIANOROLLTOOLBARVIEW_H
#define PIANOROLLTOOLBARVIEW_H

#include <QWidget>

#include "Global/ClipEditorGlobal.h"
#include "UI/Controls/EditLabel.h"
#include "UI/Controls/SeekBar.h"

class Clip;
class Button;

class ClipEditorToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ClipEditorToolBarView(QWidget *parent = nullptr);
    void setClip(Clip *clip);
    void setClipPropertyEditorEnabled(bool on);
    void setPianoRollEditToolsEnabled(bool on);

signals:
    void clipNameChanged(const QString &name);
    void editModeChanged(ClipEditorGlobal::PianoRollEditMode mode);

private slots:
    void onClipNameEdited(const QString &name);

private:
    int m_contentHeight = 28;

    Clip *m_clip = nullptr;

    EditLabel *m_elClipName;
    // Button *m_btnMute{};
    SeekBar *m_sBarGain{};

    Button *m_btnArrow;
    Button *m_btnNotePencil;
    Button *m_btnPitchPencil;
    Button *m_btnPitchAnchor;

    const QIcon icoArrowBlack = QIcon(":svg/icons/cursor_24_filled.svg");
    const QIcon icoArrowWhite = QIcon(":svg/icons/cursor_24_filled_white.svg");
    const QIcon icoNotePencilBlack = QIcon(":svg/icons/edit_24_filled.svg");
    const QIcon icoNotePencilWhite = QIcon(":svg/icons/edit_24_filled_white.svg");
    const QIcon icoPitchPencilBlack = QIcon(":svg/icons/pitch_edit_24_filled.svg");
    const QIcon icoPitchPencilWhite = QIcon(":svg/icons/pitch_edit_24_filled_white.svg");
    const QIcon icoPitchAnchorBlack = QIcon(":svg/icons/pitch_anchor_24_filled.svg");
    const QIcon icoPitchAnchorWhite = QIcon(":svg/icons/pitch_anchor_24_filled_white.svg");
};



#endif // PIANOROLLTOOLBARVIEW_H
