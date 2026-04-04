//
// Created by fluty on 24-8-1.
//

#ifndef NOTEPROPERTYEDITDIALOG_H
#define NOTEPROPERTYEDITDIALOG_H

#include "Model/AppModel/Note.h"
#include "Model/NoteDialog/NoteDialogResult.h"
#include "UI/Dialogs/Base/OKCancelDialog.h"
#include "Global/AppGlobal.h"

class QLineEdit;
class PhonemeNameListModel;
class PhonemeNameListWidget;
class LanguageComboBox;
class Note;
class Button;

class NotePropertyDialog final : public OKCancelDialog {
    Q_OBJECT

public:
    explicit NotePropertyDialog(const Note *note,
                                AppGlobal::NotePropertyType propertyType = AppGlobal::Lyric,
                                QWidget *parent = nullptr);
    [[nodiscard]] NoteDialogResult result();

private:
    bool validatePhonemes() const;
    NoteDialogResult m_result;
    LanguageComboBox *m_cbLanguage;
    QLineEdit *m_leLyric;
    QLineEdit *m_lePron;

    PhonemeNameListModel *m_phonemeNameModelOriginal;
    PhonemeNameListWidget *m_listPhonemeNamesEdited;
    PhonemeNameListModel *m_phonemeNameModelEdited;
    bool m_isPhonemeNameEdited = false;
    bool m_isResetting = false;
    Button *m_btnAddPhoneme;
    Button *m_btnResetPhonemeNames;
    
    Note *m_note;
};



#endif // NOTEPROPERTYEDITDIALOG_H
