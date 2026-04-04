//
// Created by fluty on 24-8-1.
//

#include "NotePropertyDialog.h"

#include "Model/AppModel/Note.h"
#include "PhonemeNameListWidget.h"
#include "Model/NoteDialog/PhonemeNameListModel.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Views/Common/LanguageComboBox.h"
#include "Utils/Linq.h"

#include <QFormLayout>
#include <QVBoxLayout>

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

    m_isPhonemeNameEdited = note->phonemes().nameSeq.isEdited();

    m_phonemeNameModelOriginal = new PhonemeNameListModel(this);
    m_phonemeNameModelEdited = new PhonemeNameListModel(this);

    for (const auto originalNames = note->phonemes().nameSeq.original;
         const auto &phoneme : originalNames) {
        m_phonemeNameModelOriginal->addItem(
            PhonemeNameItemModel(phoneme.language, phoneme.name, phoneme.isOnset));

        if (!m_isPhonemeNameEdited) {
            m_phonemeNameModelEdited->addItem(
                PhonemeNameItemModel(phoneme.language, phoneme.name, phoneme.isOnset));
        }
    }

    if (m_isPhonemeNameEdited) {
        for (const auto editedNames = note->phonemes().nameSeq.edited;
             const auto &phoneme : editedNames) {
            m_phonemeNameModelEdited->addItem(
                PhonemeNameItemModel(phoneme.language, phoneme.name, phoneme.isOnset));
        }
    }

    m_listPhonemeNamesEdited = new PhonemeNameListWidget;
    m_listPhonemeNamesEdited->setModel(m_phonemeNameModelEdited);
    
    m_btnAddPhoneme = new Button(tr("Add Phoneme"));
    connect(m_btnAddPhoneme, &Button::clicked, this, [this] {
        PhonemeNameItemModel newItem;
        newItem.setLanguage(m_cbLanguage->currentText());
        newItem.setName("");
        newItem.setIsOnset(false);
        m_phonemeNameModelEdited->addItem(newItem);
        m_listPhonemeNamesEdited->scrollToBottom();
    });
    
    m_btnResetPhonemeNames = new Button(tr("Reset Phonemes"));
    m_btnResetPhonemeNames->setEnabled(m_isPhonemeNameEdited);
    const auto phonemeNamesLayout = new QVBoxLayout;
    phonemeNamesLayout->addWidget(m_listPhonemeNamesEdited);
    const auto buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(m_btnAddPhoneme);
    buttonsLayout->addWidget(m_btnResetPhonemeNames);
    buttonsLayout->setContentsMargins({});
    phonemeNamesLayout->addLayout(buttonsLayout);

    const auto mainLayout = new QFormLayout;
    mainLayout->setLabelAlignment(Qt::AlignmentFlag::AlignRight | Qt::AlignmentFlag::AlignTrailing |
                                  Qt::AlignmentFlag::AlignVCenter);
    // mainLayout->addRow(tr("Start:"), sbStart);
    // mainLayout->addRow(tr("Length:"), sbLength);
    mainLayout->addRow(tr("Language:"), m_cbLanguage);
    mainLayout->addRow(tr("Lyric:"), m_leLyric);
    mainLayout->addRow(tr("Pronunciation:"), m_lePron);
    mainLayout->addRow(tr("Phonemes:"), phonemeNamesLayout);
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
            // m_lePhonemeAhead->setFocus(Qt::TabFocusReason);
            break;
    }
    connect(okButton(), &AccentButton::clicked, this, &Dialog::accept);
    connect(cancelButton(), &AccentButton::clicked, this, &Dialog::reject);

    connect(m_phonemeNameModelEdited, &QAbstractItemModel::dataChanged, this, [this] {
        if (m_isResetting)
            return;
        m_isPhonemeNameEdited = true;
        m_btnResetPhonemeNames->setEnabled(true);
    });
    connect(m_phonemeNameModelEdited, &QAbstractItemModel::rowsInserted, this, [this] {
        if (m_isResetting)
            return;
        m_isPhonemeNameEdited = true;
        m_btnResetPhonemeNames->setEnabled(true);
    });
    connect(m_phonemeNameModelEdited, &QAbstractItemModel::rowsRemoved, this, [this] {
        if (m_isResetting)
            return;
        m_isPhonemeNameEdited = true;
        m_btnResetPhonemeNames->setEnabled(true);
    });
    connect(m_phonemeNameModelEdited, &QAbstractItemModel::modelReset, this, [this] {
        if (m_isResetting)
            return;
        m_isPhonemeNameEdited = true;
        m_btnResetPhonemeNames->setEnabled(true);
    });

    connect(m_btnResetPhonemeNames, &Button::clicked, this, [this] {
        m_isResetting = true;
        m_phonemeNameModelEdited->setItems(m_phonemeNameModelOriginal->items());
        m_isResetting = false;
        m_isPhonemeNameEdited = false;
        m_btnResetPhonemeNames->setEnabled(false);
    });
}

NoteDialogResult NotePropertyDialog::result() {
    m_result.language = m_cbLanguage->currentText();
    m_result.lyric = m_leLyric->text();
    m_result.pronunciation.edited = m_lePron->text();

    if (m_isPhonemeNameEdited) {
        m_result.phonemeNameSeq.edited = Linq::selectMany(m_phonemeNameModelEdited->items(),
                                                          [](const PhonemeNameItemModel &item) {
                                                              PhonemeName name;
                                                              name.language = item.language();
                                                              name.name = item.name();
                                                              name.isOnset = item.isOnset();
                                                              return name;
                                                          });
    } else {
        m_result.phonemeNameSeq.edited = {};
    }
    m_result.isPhonemeNameEdited = m_isPhonemeNameEdited;
    return m_result;
}