//
// Created by fluty on 24-8-2.
//

#ifndef CLIPEDITORTOOLBARVIEW_P_H
#define CLIPEDITORTOOLBARVIEW_P_H

#include "Global/AppGlobal.h"
#include "Global/ClipEditorGlobal.h"

#include <QObject>

class SingingClip;
class LanguageComboBox;
class Clip;
class QAbstractButton;
class SeekBar;
class EditLabel;
class Button;
class ClipEditorToolBarView;
class ClipEditorToolBarViewPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(ClipEditorToolBarView)
public:
    explicit ClipEditorToolBarViewPrivate(ClipEditorToolBarView *parent) : q_ptr(parent) {
    }

    void moveToNullClipState() const;
    void moveToSingingClipState() const;
    void moveToAudioClipState() const;

    void setPianoRollToolsEnabled(bool on) const;
    int m_contentHeight = 28;

    Clip *m_clip = nullptr;
    SingingClip *m_singingClip = nullptr;
    PianoRollEditMode m_editMode = Select;

    EditLabel *m_elClipName = nullptr;
    // Button *m_btnMute;
    // SeekBar *m_sBarGain;

    Button *m_btnArrow = nullptr;
    Button *m_btnNotePencil = nullptr;
    Button *m_btnPitchPencil = nullptr;
    Button *m_btnPitchAnchor = nullptr;

    LanguageComboBox *m_cbLanguage = nullptr;

public slots:
    void onPianoRollToolButtonToggled(QAbstractButton *button, bool checked);
    void onClipNameEdited(const QString &name) const;
    void onClipPropertyChanged() const;
    void onClipLanguageChanged(AppGlobal::languageType language) const;
    void onLanguageEdited(int index);

private:
    ClipEditorToolBarView *q_ptr;
};


#endif // CLIPEDITORTOOLBARVIEW_P_H
