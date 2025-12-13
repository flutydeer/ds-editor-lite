//
// Created by fluty on 24-8-2.
//

#ifndef CLIPEDITORTOOLBARVIEW_P_H
#define CLIPEDITORTOOLBARVIEW_P_H

#include "Global/AppGlobal.h"

#include <QButtonGroup>
#include <QObject>

class SingingClip;
class LanguageComboBox;
class Clip;
class QAbstractButton;
class SeekBar;
class LineEdit;
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

    [[nodiscard]] Button *buildToolButton(const QString &objName, const QString &tipTitle,
                                          const QKeySequence &shortcut = QKeySequence(),
                                          const QString &tipDesc = QString()) const;
    Button *buildCommonButton(const QString &objName, const QString &tipTitle,
                              const QKeySequence &shortcut = QKeySequence(),
                              const QString &tipDesc = QString()) const;

    void setPianoRollToolsEnabled(bool on) const;
    int m_contentHeight = 28;

    Clip *m_clip = nullptr;
    SingingClip *m_singingClip = nullptr;
    PianoRollEditMode m_editMode = Select;

    LineEdit *m_leClipName = nullptr;
    // Button *m_btnMute;
    // SeekBar *m_sBarGain;

    QButtonGroup *m_toolButtonGroup = nullptr;
    Button *m_btnArrow = nullptr;
    Button *m_btnBeam = nullptr;
    Button *m_btnNotePencil = nullptr;
    Button *m_btnNoteEraser = nullptr;
    Button *m_btnNoteSplit = nullptr;
    Button *m_btnPitchPencil = nullptr;
    // Button *m_btnPitchAnchor = nullptr;
    Button *m_btnPitchEraser = nullptr;
    // Button *m_btnFreezePitch = nullptr;

    LanguageComboBox *m_cbClipLanguage = nullptr;

public slots:
    void onPianoRollToolButtonToggled(const QAbstractButton *button, bool checked);
    void onClipNameEdited() const;
    void onClipPropertyChanged() const;
    void onClipLanguageChanged(const QString &language) const;
    void onLanguageEdited(const QString &language) const;

private:
    ClipEditorToolBarView *q_ptr;
};


#endif // CLIPEDITORTOOLBARVIEW_P_H
