//
// Created by fluty on 2024/2/10.
//

#ifndef PIANOROLLTOOLBARVIEW_H
#define PIANOROLLTOOLBARVIEW_H

#include <QWidget>

#include "Global/ClipEditorGlobal.h"


class EditLabel;
class SeekBar;
class QAbstractButton;
class Clip;
class Button;

using namespace ClipEditorGlobal;

class ClipEditorToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ClipEditorToolBarView(QWidget *parent = nullptr);
    void setDataContext(Clip *clip);
    [[nodiscard]] PianoRollEditMode editMode() const;

signals:
    void editModeChanged(ClipEditorGlobal::PianoRollEditMode mode);

private slots:
    void onPianoRollToolButtonToggled(QAbstractButton *button, bool checked);
    void onClipNameEdited(const QString &name) const;
    void onClipPropertyChanged();

private:
    void moveToNullClipState() const;
    void moveToSingingClipState() const;
    void moveToAudioClipState() const;

    void setPianoRollToolsEnabled(bool on) const;
    int m_contentHeight = 28;

    Clip *m_clip = nullptr;
    PianoRollEditMode m_editMode = Select;

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
