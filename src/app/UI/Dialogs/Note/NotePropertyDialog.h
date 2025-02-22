//
// Created by fluty on 24-8-1.
//

#ifndef NOTEPROPERTYEDITDIALOG_H
#define NOTEPROPERTYEDITDIALOG_H

#include "Model/AppModel/Note.h"
#include "Model/NoteDialogResult/NoteDialogResult.h"
#include "UI/Dialogs/Base/OKCancelDialog.h"
#include "Global/AppGlobal.h"

class QLineEdit;
class LanguageComboBox;
class Note;

class NotePropertyDialog final : public OKCancelDialog {
    Q_OBJECT

public:
    explicit NotePropertyDialog(Note *note,
                                AppGlobal::NotePropertyType propertyType = AppGlobal::Lyric,
                                QWidget *parent = nullptr);
    [[nodiscard]] NoteDialogResult result();

private:
    NoteDialogResult m_result;
    LanguageComboBox *m_cbLanguage;
    QLineEdit *m_leLyric;
    QLineEdit *m_lePron;
    QLineEdit *m_lePhonemeAhead;
    QLineEdit *m_lePhonemeNormal;
    Note *m_note;

    static QString phonemesToString(const QList<QString> &phonemes);
    static QStringList phonemesFromString(const QString &names);
};



#endif // NOTEPROPERTYEDITDIALOG_H
