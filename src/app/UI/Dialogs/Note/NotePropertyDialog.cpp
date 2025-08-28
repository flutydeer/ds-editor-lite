//
// Created by fluty on 24-8-1.
//

#include "NotePropertyDialog.h"

#include "Model/AppModel/Note.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Views/Common/LanguageComboBox.h"
#include "Utils/Linq.h"

#include <QFormLayout>

NotePropertyDialog::NotePropertyDialog(const Note *note,
                                       const AppGlobal::NotePropertyType propertyType,
                                       QWidget *parent)
    : OKCancelDialog(parent), m_note() {
    setWindowTitle(tr("Note Properties - %1").arg(note->lyric()));
    setFocusPolicy(Qt::ClickFocus);

    m_cbLanguage = new LanguageComboBox(note->language());

    m_leLyric = new QLineEdit(note->lyric());
    m_leLyric->setPlaceholderText(note->lyric());

    m_result.pronunciation = note->pronunciation();
    m_lePron = new QLineEdit(note->pronunciation().edited);
    m_lePron->setPlaceholderText(note->pronunciation().original);

    m_result.phonemeNameInfo = note->phonemes().nameInfo;
    const auto nameInfo = m_result.phonemeNameInfo;

    m_lePhonemeAhead = new QLineEdit(phonemesToString(nameInfo.ahead.edited));
    m_lePhonemeAhead->setPlaceholderText(phonemesToString(nameInfo.ahead.original));

    m_lePhonemeNormal = new QLineEdit(phonemesToString(nameInfo.normal.edited));
    m_lePhonemeNormal->setPlaceholderText(phonemesToString(nameInfo.normal.original));

    const auto mainLayout = new QFormLayout;
    mainLayout->setLabelAlignment(Qt::AlignmentFlag::AlignRight | Qt::AlignmentFlag::AlignTrailing |
                                  Qt::AlignmentFlag::AlignVCenter);
    // mainLayout->addRow(tr("Start:"), sbStart);
    // mainLayout->addRow(tr("Length:"), sbLength);
    mainLayout->addRow(tr("Language:"), m_cbLanguage);
    mainLayout->addRow(tr("Lyric:"), m_leLyric);
    mainLayout->addRow(tr("Pronunciation:"), m_lePron);
    mainLayout->addRow(tr("Ahead Phonemes:"), m_lePhonemeAhead);
    mainLayout->addRow(tr("Normal Phonemes:"), m_lePhonemeNormal);
    mainLayout->setContentsMargins({});

    body()->setLayout(mainLayout);

    setModal(true);
    switch (propertyType) {
        case AppGlobal::Language:
            m_cbLanguage->setFocus(Qt::TabFocusReason);
            break;
        case AppGlobal::Lyric:
            m_leLyric->setFocus(Qt::TabFocusReason);
            break;
        case AppGlobal::Pronunciation:
            m_lePron->setFocus(Qt::TabFocusReason);
            break;
        case AppGlobal::Phonemes:
            m_lePhonemeAhead->setFocus(Qt::TabFocusReason);
            break;
    }
    connect(okButton(), &AccentButton::clicked, this, &Dialog::accept);
    connect(cancelButton(), &AccentButton::clicked, this, &Dialog::reject);
}

NoteDialogResult NotePropertyDialog::result() {
    m_result.language = m_cbLanguage->currentText();
    m_result.lyric = m_leLyric->text();
    m_result.pronunciation.edited = m_lePron->text();

    const auto aheadText = m_lePhonemeAhead->text();
    if (!aheadText.isNull() && !aheadText.isEmpty()) {
        const auto aheadList = phonemesFromString(aheadText);
        m_result.phonemeNameInfo.ahead.edited = aheadList;
    } else
        m_result.phonemeNameInfo.ahead.edited = QList<QString>();

    const auto normalText = m_lePhonemeNormal->text();
    if (!normalText.isNull() && !normalText.isEmpty()) {
        const auto normalList = phonemesFromString(m_lePhonemeNormal->text());
        m_result.phonemeNameInfo.normal.edited = normalList;
    } else
        m_result.phonemeNameInfo.normal.edited = QList<QString>();

    return m_result;
}

QString NotePropertyDialog::phonemesToString(const QList<QString> &phonemes) {
    QString result;
    int i = 0;
    for (const auto &phoneme : phonemes) {
        result.append(phoneme);
        if (i < phonemes.count() - 1)
            result.append(" ");
        i++;
    }
    return result;
}

QStringList NotePropertyDialog::phonemesFromString(const QString &names) {
    return names.split(" ");
}