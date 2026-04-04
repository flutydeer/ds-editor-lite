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
    [[nodiscard]] bool validatePhonemes() const;
    NoteDialogResult m_result;
    LanguageComboBox *m_cbLanguage;
    QLineEdit *m_leLyric;
    QLineEdit *m_lePron;

    PhonemeNameListModel *m_phoneModelOriginal;
    PhonemeNameListWidget *m_listPhonesEdited;
    PhonemeNameListModel *m_phoneModelEdited;
    bool m_isPhoneEdited = false;
    bool m_isResetting = false;
    Button *m_btnAddPhone;
    Button *m_btnResetPhones;
    
    Note *m_note;
};



#endif // NOTEPROPERTYEDITDIALOG_H
