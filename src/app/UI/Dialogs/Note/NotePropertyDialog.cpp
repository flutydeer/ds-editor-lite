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
#include "UI/Controls/Toast.h"

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

    m_isPhoneEdited = note->phonemes().nameSeq.isEdited();

    m_phoneModelOriginal = new PhonemeNameListModel(this);
    m_phoneModelEdited = new PhonemeNameListModel(this);

    for (const auto originalNames = note->phonemes().nameSeq.original;
         const auto &phoneme : originalNames) {
        m_phoneModelOriginal->addItem(
            PhonemeNameItemModel(phoneme.language, phoneme.name, phoneme.isOnset));

        if (!m_isPhoneEdited) {
            m_phoneModelEdited->addItem(
                PhonemeNameItemModel(phoneme.language, phoneme.name, phoneme.isOnset));
        }
    }

    if (m_isPhoneEdited) {
        for (const auto editedNames = note->phonemes().nameSeq.edited;
             const auto &phoneme : editedNames) {
            m_phoneModelEdited->addItem(
                PhonemeNameItemModel(phoneme.language, phoneme.name, phoneme.isOnset));
        }
    }

    m_listPhonesEdited = new PhonemeNameListWidget;
    m_listPhonesEdited->setModel(m_phoneModelEdited);

    m_btnAddPhone = new Button(tr("Add Phone"));
    connect(m_btnAddPhone, &Button::clicked, this, [this] {
        PhonemeNameItemModel newItem;
        newItem.setLanguage(m_cbLanguage->currentText());
        newItem.setName("");
        newItem.setIsOnset(false);
        m_phoneModelEdited->addItem(newItem);
        m_listPhonesEdited->scrollToBottom();
    });

    m_btnResetPhones = new Button(tr("Reset Phones"));
    m_btnResetPhones->setEnabled(m_isPhoneEdited);
    const auto phonesLayout = new QVBoxLayout;
    phonesLayout->addWidget(m_listPhonesEdited);
    const auto buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(m_btnAddPhone);
    buttonsLayout->addWidget(m_btnResetPhones);
    buttonsLayout->setContentsMargins({});
    phonesLayout->addLayout(buttonsLayout);

    const auto mainLayout = new QFormLayout;
    mainLayout->setLabelAlignment(Qt::AlignmentFlag::AlignRight | Qt::AlignmentFlag::AlignTrailing |
                                  Qt::AlignmentFlag::AlignVCenter);
    // mainLayout->addRow(tr("Start:"), sbStart);
    // mainLayout->addRow(tr("Length:"), sbLength);
    mainLayout->addRow(tr("Language:"), m_cbLanguage);
    mainLayout->addRow(tr("Lyric:"), m_leLyric);
    mainLayout->addRow(tr("Pronunciation:"), m_lePron);
    mainLayout->addRow(tr("Phones:"), phonesLayout);
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
    connect(okButton(), &AccentButton::clicked, this, [this] {
        if (validatePhonemes())
            accept();
    });
    connect(cancelButton(), &AccentButton::clicked, this, &Dialog::reject);

    connect(m_phoneModelEdited, &QAbstractItemModel::dataChanged, this, [this] {
        if (m_isResetting)
            return;
        m_isPhoneEdited = true;
        m_btnResetPhones->setEnabled(true);
    });
    connect(m_phoneModelEdited, &QAbstractItemModel::rowsInserted, this, [this] {
        if (m_isResetting)
            return;
        m_isPhoneEdited = true;
        m_btnResetPhones->setEnabled(true);
    });
    connect(m_phoneModelEdited, &QAbstractItemModel::rowsRemoved, this, [this] {
        if (m_isResetting)
            return;
        m_isPhoneEdited = true;
        m_btnResetPhones->setEnabled(true);
    });
    connect(m_phoneModelEdited, &QAbstractItemModel::modelReset, this, [this] {
        if (m_isResetting)
            return;
        m_isPhoneEdited = true;
        m_btnResetPhones->setEnabled(true);
    });

    connect(m_btnResetPhones, &Button::clicked, this, [this] {
        m_isResetting = true;
        m_phoneModelEdited->setItems(m_phoneModelOriginal->items());
        m_isResetting = false;
        m_isPhoneEdited = false;
        m_btnResetPhones->setEnabled(false);
    });
}

NoteDialogResult NotePropertyDialog::result() {
    m_result.language = m_cbLanguage->currentText();
    m_result.lyric = m_leLyric->text();
    m_result.pronunciation.edited = m_lePron->text();

    if (m_isPhoneEdited) {
        m_result.phonemeNameSeq.edited = Linq::selectMany(m_phoneModelEdited->items(),
                                                          [](const PhonemeNameItemModel &item) {
                                                              PhonemeName name;
                                                              name.language = item.language();
                                                              name.name = item.name().trimmed();
                                                              name.isOnset = item.isOnset();
                                                              return name;
                                                          });
    } else {
        m_result.phonemeNameSeq.edited = {};
    }
    m_result.isPhonemeNameEdited = m_isPhoneEdited;
    return m_result;
}

bool NotePropertyDialog::validatePhonemes() const {
    if (m_isPhoneEdited) {
        if (m_phoneModelEdited->items().isEmpty()) {
            Toast::show(tr("Please add phonemes."));
            return false;
        }

        for (const auto &item : m_phoneModelEdited->items()) {
            if (item.language().isEmpty() || item.name().isEmpty()) {
                Toast::show(tr("Please fill in name for all phonemes."));
                return false;
            }
        }

        bool hasOnset = false;
        for (const auto &item : m_phoneModelEdited->items()) {
            if (item.isOnset()) {
                hasOnset = true;
                break;
            }
        }
        if (!hasOnset) {
            Toast::show(tr("At least one phoneme must be set as onset."));
            return false;
        }
    }
    return true;
}
