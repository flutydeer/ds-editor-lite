//
// Created by FlutyDeer on 2026/7/12.
//

#ifndef DS_EDITOR_LITE_PHONEMEEDITORDIALOG_H
#define DS_EDITOR_LITE_PHONEMEEDITORDIALOG_H

#include "Model/AppModel/Phonemes.h"
#include "UI/Dialogs/Base/OKCancelDialog.h"

#include <QList>

class PhonemeNameListModel;
class PhonemeNameListWidget;
class Note;
class Button;

class PhonemeEditorDialog final : public OKCancelDialog {
    Q_OBJECT

public:
    explicit PhonemeEditorDialog(const Note *note, QWidget *parent = nullptr);
    [[nodiscard]] QList<PhonemeName> phonemeNames() const;

private:
    [[nodiscard]] bool validatePhonemes() const;

    QString m_noteLanguage;
    PhonemeNameListModel *m_phoneModelOriginal;
    PhonemeNameListWidget *m_listPhonesEdited;
    PhonemeNameListModel *m_phoneModelEdited;
    bool m_isPhoneEdited = false;
    bool m_isResetting = false;
    Button *m_btnAddPhone;
    Button *m_btnResetPhones;
};

#endif // DS_EDITOR_LITE_PHONEMEEDITORDIALOG_H