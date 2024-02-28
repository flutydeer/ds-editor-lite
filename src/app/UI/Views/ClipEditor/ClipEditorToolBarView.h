//
// Created by fluty on 2024/2/10.
//

#ifndef PIANOROLLTOOLBARVIEW_H
#define PIANOROLLTOOLBARVIEW_H

#include <QWidget>

#include "Global/ClipEditorGlobal.h"
#include "UI/Controls/EditLabel.h"
#include "UI/Controls/SeekBar.h"

class QPushButton;

class ClipEditorToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ClipEditorToolBarView(QWidget *parent = nullptr);
    void setClipName(const QString &name);
    void setClipPropertyEditorEnabled(bool on);
    void setPianoRollEditToolsEnabled(bool on);

signals:
    void clipNameChanged(const QString &name);
    void editModeChanged(ClipEditorGlobal::PianoRollEditMode mode);

private:
    int m_contentHeight = 28;

    EditLabel *m_elClipName;
    QPushButton *m_btnMute{};
    SeekBar *m_sBarGain{};

    QPushButton *m_btnArrow;
    QPushButton *m_btnNotePencil;
    QPushButton *m_btnPitchPencil;
    QPushButton *m_btnPitchAnchor;

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
